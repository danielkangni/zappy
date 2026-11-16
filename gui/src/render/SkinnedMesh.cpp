#define TINYGLTF_IMPLEMENTATION
#define TINYGLTF_NO_INCLUDE_JSON
#define TINYGLTF_NO_STB_IMAGE
#define TINYGLTF_NO_STB_IMAGE_WRITE
#include "GUIException.hpp"
#include <json.hpp>
using json = nlohmann::json;
#include <tiny_gltf.h>

#include "render/SkinnedMesh.hpp"
#include "AssetCache.hpp"
#include <SFML/Graphics/Image.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/quaternion.hpp>
#include <stdexcept>
#include <cstring>
#include <algorithm>
#include <map>
#include <iostream>
#include <fstream>
#include <sys/stat.h>

static GLuint makeTexFromImageBytes(const std::string& bytes);

static int findInterval(const std::vector<float>& times, float t)
{
    if (times.size() < 2) return 0;
    for (int i = 0; i < (int)times.size() - 1; ++i)
        if (t <= times[i+1]) return i;
    return (int)times.size() - 2;
}

template<class T>
static T sampleLinear(const std::vector<float>& times,
                      const std::vector<T>& keys, float t)
{
    if (keys.empty()) return T{};
    if (keys.size() == 1) return keys[0];
    int i = findInterval(times, t);
    float dt = times[i+1] - times[i];
    float f  = (dt > 1e-6f) ? (t - times[i]) / dt : 0.f;
    return glm::mix(keys[i], keys[i+1], f);
}

static glm::quat sampleSlerp(const std::vector<float>& times,
                              const std::vector<glm::quat>& keys, float t)
{
    if (keys.empty()) return glm::quat(1,0,0,0);
    if (keys.size() == 1) return keys[0];
    int i = findInterval(times, t);
    float dt = times[i+1] - times[i];
    float f  = (dt > 1e-6f) ? (t - times[i]) / dt : 0.f;
    return glm::slerp(keys[i], keys[i+1], f);
}

static const float* floatPtr(const tinygltf::Model& m, int accIdx)
{
    auto& acc = m.accessors[accIdx];
    auto& bv  = m.bufferViews[acc.bufferView];
    auto& buf = m.buffers[bv.buffer];
    return reinterpret_cast<const float*>(
        buf.data.data() + bv.byteOffset + acc.byteOffset);
}

static const uint16_t* u16Ptr(const tinygltf::Model& m, int accIdx)
{
    auto& acc = m.accessors[accIdx];
    auto& bv  = m.bufferViews[acc.bufferView];
    auto& buf = m.buffers[bv.buffer];
    return reinterpret_cast<const uint16_t*>(
        buf.data.data() + bv.byteOffset + acc.byteOffset);
}

static std::vector<unsigned int> getIndices(const tinygltf::Model& m, int accIdx)
{
    auto& acc = m.accessors[accIdx];
    auto& bv  = m.bufferViews[acc.bufferView];
    auto& buf = m.buffers[bv.buffer];
    const uint8_t* base = buf.data.data() + bv.byteOffset + acc.byteOffset;
    std::vector<unsigned int> out(acc.count);
    for (size_t i = 0; i < acc.count; ++i) {
        if      (acc.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT)
            out[i] = reinterpret_cast<const uint16_t*>(base)[i];
        else if (acc.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT)
            out[i] = reinterpret_cast<const unsigned int*>(base)[i];
        else if (acc.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE)
            out[i] = base[i];
    }
    return out;
}

static const uint32_t CACHE_MAGIC   = 0x43424C47;
static const uint32_t CACHE_VERSION = 7;

static std::string cachePath(const std::string& glbPath) {
    return glbPath + ".glbc";
}

template<class T>
static void cw(std::ofstream& f, const T& v) {
    f.write(reinterpret_cast<const char*>(&v), sizeof(T));
}
template<class T>
static void cwv(std::ofstream& f, const std::vector<T>& v) {
    uint32_t n = (uint32_t)v.size();
    cw(f, n);
    if (n) f.write(reinterpret_cast<const char*>(v.data()), n * sizeof(T));
}
template<class T>
static bool cr(std::ifstream& f, T& v) {
    return (bool)f.read(reinterpret_cast<char*>(&v), sizeof(T));
}
template<class T>
static bool crv(std::ifstream& f, std::vector<T>& v) {
    uint32_t n = 0;
    if (!cr(f, n)) return false;

    std::streampos cur = f.tellg();
    f.seekg(0, std::ios::end);
    std::streampos end = f.tellg();
    f.seekg(cur);
    if (cur < 0 || end < 0) return false;
    if ((uint64_t)n * sizeof(T) > (uint64_t)(end - cur)) return false;
    v.resize(n);
    if (n) f.read(reinterpret_cast<char*>(v.data()), n * sizeof(T));
    return f.good();
}

static void saveAnimsToCache(std::ofstream& f, const std::vector<Animation>& anims)
{
    uint32_t na = (uint32_t)anims.size();
    cw(f, na);
    for (auto& a : anims) {
        uint32_t nl = (uint32_t)a.name.size();
        cw(f, nl);
        f.write(a.name.c_str(), nl);
        cw(f, a.duration);
        uint32_t nc = (uint32_t)a.channels.size();
        cw(f, nc);
        for (auto& bc : a.channels) {
            cw(f, bc.boneIndex);
            cwv(f, bc.timesT);

            uint32_t nt = (uint32_t)bc.transKeys.size();
            cw(f, nt);
            for (auto& v : bc.transKeys) { cw(f,v.x); cw(f,v.y); cw(f,v.z); }
            cwv(f, bc.timesR);
            uint32_t nr = (uint32_t)bc.rotKeys.size();
            cw(f, nr);
            for (auto& q : bc.rotKeys) { cw(f,q.w); cw(f,q.x); cw(f,q.y); cw(f,q.z); }
            cwv(f, bc.timesS);
            uint32_t ns = (uint32_t)bc.scaleKeys.size();
            cw(f, ns);
            for (auto& v : bc.scaleKeys) { cw(f,v.x); cw(f,v.y); cw(f,v.z); }
        }
    }
}

static bool loadAnimsFromCache(std::ifstream& f, std::vector<Animation>& anims)
{
    uint32_t na = 0; if (!cr(f, na)) return false;
    anims.resize(na);
    for (auto& a : anims) {
        uint32_t nl = 0; cr(f, nl);
        a.name.resize(nl);
        f.read(&a.name[0], nl);
        cr(f, a.duration);
        uint32_t nc = 0; cr(f, nc);
        a.channels.resize(nc);
        for (auto& bc : a.channels) {
            cr(f, bc.boneIndex);
            crv(f, bc.timesT);
            uint32_t nt = 0; cr(f, nt);
            bc.transKeys.resize(nt);
            for (auto& v : bc.transKeys) { cr(f,v.x); cr(f,v.y); cr(f,v.z); }
            crv(f, bc.timesR);
            uint32_t nr = 0; cr(f, nr);
            bc.rotKeys.resize(nr);
            for (auto& q : bc.rotKeys) { cr(f,q.w); cr(f,q.x); cr(f,q.y); cr(f,q.z); }
            crv(f, bc.timesS);
            uint32_t ns = 0; cr(f, ns);
            bc.scaleKeys.resize(ns);
            for (auto& v : bc.scaleKeys) { cr(f,v.x); cr(f,v.y); cr(f,v.z); }
        }
    }
    return f.good();
}

bool tryLoadCache(const std::string& glbPath, SkinnedMesh& sm,
                  std::vector<float>& verts, std::vector<unsigned int>& indices)
{
    auto cp = cachePath(glbPath);
    struct stat gs{}, cs{};
    if (stat(glbPath.c_str(), &gs) != 0) return false;
    if (stat(cp.c_str(), &cs) != 0) return false;
    if (cs.st_mtime < gs.st_mtime) return false;

    std::ifstream f(cp, std::ios::binary);
    if (!f) return false;
    uint32_t magic = 0, version = 0;
    cr(f, magic); cr(f, version);
    if (magic != CACHE_MAGIC || version != CACHE_VERSION) return false;

    cr(f, sm.bmin_.x); cr(f, sm.bmin_.y); cr(f, sm.bmin_.z);
    cr(f, sm.bmax_.x); cr(f, sm.bmax_.y); cr(f, sm.bmax_.z);
    if (!crv(f, verts))   return false;
    if (!crv(f, indices)) return false;

    uint32_t nj = 0; cr(f, nj);
    sm.invBindMats_.resize(nj);
    for (auto& m : sm.invBindMats_)
        f.read(reinterpret_cast<char*>(glm::value_ptr(m)), 64);
    crv(f, sm.parentOf_);
    f.read(reinterpret_cast<char*>(glm::value_ptr(sm.skinRootXform_)), 64);

    if (!loadAnimsFromCache(f, sm.animations_)) return false;
    uint32_t nb = 0; cr(f, nb);
    sm.boneNames_.resize(nb);
    for (auto& name : sm.boneNames_) {
        uint32_t nl = 0; cr(f, nl);
        name.resize(nl);
        if (nl) f.read(&name[0], nl);
    }

    uint32_t nsm = 0; cr(f, nsm);
    sm.subMeshes_.resize(nsm);
    std::vector<int32_t> smImgSrc(nsm, -1);
    for (uint32_t i = 0; i < nsm; ++i) {
        cr(f, sm.subMeshes_[i].start);
        cr(f, sm.subMeshes_[i].count);
        cr(f, smImgSrc[i]);
    }

    uint32_t ni = 0; cr(f, ni);
    std::map<int32_t, GLuint> srcTex;
    for (uint32_t i = 0; i < ni; ++i) {
        int32_t src = -1; cr(f, src);
        uint32_t blen = 0; cr(f, blen);
        std::string bytes;
        if (blen) {
            bytes.resize(blen);
            f.read(&bytes[0], blen);
        }
        if (src >= 0 && blen > 0) {
            GLuint t = makeTexFromImageBytes(bytes);
            if (t) { srcTex[src] = t; sm.matTextures_.push_back(t); }
        }
    }
    for (uint32_t i = 0; i < nsm; ++i) {
        auto it = srcTex.find(smImgSrc[i]);
        if (it != srcTex.end()) sm.subMeshes_[i].tex = it->second;
    }
    return f.good();
}

void saveCache(const std::string& glbPath, const SkinnedMesh& sm,
               const std::vector<float>& verts,
               const std::vector<unsigned int>& indices,
               const std::vector<SubMeshMeta>& smeta,
               const std::map<int, std::string>& imgBytes)
{

    const std::string finalPath = cachePath(glbPath);
    const std::string tmpPath   = finalPath + ".tmp";
    {
        std::ofstream f(tmpPath, std::ios::binary | std::ios::trunc);
        if (!f) return;
        cw(f, CACHE_MAGIC);
        cw(f, CACHE_VERSION);
        cw(f, sm.bmin_.x); cw(f, sm.bmin_.y); cw(f, sm.bmin_.z);
        cw(f, sm.bmax_.x); cw(f, sm.bmax_.y); cw(f, sm.bmax_.z);
        cwv(f, verts);
        cwv(f, indices);
        uint32_t nj = (uint32_t)sm.invBindMats_.size();
        cw(f, nj);
        for (auto& m : sm.invBindMats_)
            f.write(reinterpret_cast<const char*>(glm::value_ptr(m)), 64);
        cwv(f, sm.parentOf_);
        f.write(reinterpret_cast<const char*>(glm::value_ptr(sm.skinRootXform_)), 64);
        saveAnimsToCache(f, sm.animations_);
        uint32_t nb = (uint32_t)sm.boneNames_.size();
        cw(f, nb);
        for (auto& name : sm.boneNames_) {
            uint32_t nl = (uint32_t)name.size();
            cw(f, nl);
            f.write(name.c_str(), nl);
        }

        uint32_t nsm = (uint32_t)smeta.size();
        cw(f, nsm);
        for (auto& s : smeta) {
            cw(f, s.start); cw(f, s.count); cw(f, s.imgSrc);
        }

        uint32_t ni = (uint32_t)imgBytes.size();
        cw(f, ni);
        for (auto& [src, bytes] : imgBytes) {
            cw(f, (int32_t)src);
            uint32_t blen = (uint32_t)bytes.size();
            cw(f, blen);
            if (blen) f.write(bytes.data(), blen);
        }
        f.flush();
        if (!f.good()) { f.close(); std::remove(tmpPath.c_str()); return; }
    }
    if (std::rename(tmpPath.c_str(), finalPath.c_str()) != 0)
        std::remove(tmpPath.c_str());
}

static void saveAnimCache(const std::string& glbPath, const std::vector<Animation>& anims)
{
    auto cp = glbPath + ".glbca";
    std::ofstream f(cp, std::ios::binary);
    if (!f) return;
    cw(f, CACHE_MAGIC);
    uint32_t ver2 = CACHE_VERSION + 100;
    cw(f, ver2);
    saveAnimsToCache(f, anims);
}

static bool tryLoadAnimOnlyCache(const std::string& glbPath, std::vector<Animation>& out)
{
    auto cp = glbPath + ".glbca";
    struct stat gs{}, cs{};
    if (stat(glbPath.c_str(), &gs) != 0) return false;
    if (stat(cp.c_str(), &cs) != 0) return false;
    if (cs.st_mtime < gs.st_mtime) return false;
    std::ifstream f(cp, std::ios::binary);
    if (!f) return false;
    uint32_t magic = 0, version = 0;
    cr(f, magic); cr(f, version);
    if (magic != CACHE_MAGIC || version != CACHE_VERSION + 100) return false;
    return loadAnimsFromCache(f, out) && f.good();
}

static thread_local std::string g_gltfBaseDir;

static void setCdnFs(tinygltf::TinyGLTF& loader)
{
    tinygltf::FsCallbacks fs;
    fs.FileExists     = [](const std::string&, void*) { return true; };
    fs.ExpandFilePath = [](const std::string& p, void*) { return p; };
    fs.ReadWholeFile  = [](std::vector<unsigned char>* out, std::string* err,
                           const std::string& fp, void*) {
        std::string b = zappy::assetBytes(fp);
        if (b.empty() && fp.rfind("assets/", 0) != 0)
            b = zappy::assetBytes(g_gltfBaseDir + fp);
        if (b.empty()) { if (err) *err = "cdn read failed: " + fp; return false; }
        out->assign(b.begin(), b.end());
        return true;
    };
    fs.WriteWholeFile = [](std::string*, const std::string&,
                           const std::vector<unsigned char>&, void*) { return false; };
    loader.SetFsCallbacks(fs);
}

static std::string base64(const std::string& in)
{
    static const char* T = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    out.reserve((in.size() + 2) / 3 * 4);
    size_t i = 0;
    for (; i + 2 < in.size(); i += 3) {
        unsigned n = (unsigned char)in[i] << 16 | (unsigned char)in[i+1] << 8 | (unsigned char)in[i+2];
        out += T[(n>>18)&63]; out += T[(n>>12)&63]; out += T[(n>>6)&63]; out += T[n&63];
    }
    if (i < in.size()) {
        unsigned n = (unsigned char)in[i] << 16;
        if (i + 1 < in.size()) n |= (unsigned char)in[i+1] << 8;
        out += T[(n>>18)&63]; out += T[(n>>12)&63];
        out += (i + 1 < in.size()) ? T[(n>>6)&63] : '=';
        out += '=';
    }
    return out;
}

static std::string embedGltfExternals(const std::string& gltf, const std::string& dir)
{
    json j = json::parse(gltf, nullptr, false);
    if (j.is_discarded()) return gltf;
    auto embed = [&](const char* key) {
        if (!j.contains(key) || !j[key].is_array()) return;
        for (auto& e : j[key]) {
            if (!e.contains("uri") || !e["uri"].is_string()) continue;
            std::string uri = e["uri"].get<std::string>();
            if (uri.empty() || uri.rfind("data:", 0) == 0) continue;
            std::string bytes = zappy::assetBytes(dir + uri);
            if (bytes.empty()) continue;
            e["uri"] = "data:application/octet-stream;base64," + base64(bytes);
        }
    };
    embed("buffers");
    embed("images");
    return j.dump();
}

static bool loadGltfModel(const std::string& path, tinygltf::Model& model,
                          std::string& err, std::string& warn)
{
    std::string bytes = zappy::assetBytes(path);
    if (bytes.empty()) return false;
    std::string dir = path.substr(0, path.find_last_of('/') + 1);
    g_gltfBaseDir = dir;
    tinygltf::TinyGLTF loader;
    loader.SetImageLoader([](tinygltf::Image*, const int, std::string*, std::string*,
                             int, int, const unsigned char*, int, void*) { return true; }, nullptr);
    setCdnFs(loader);
    bool glb = path.size() > 4 && path.substr(path.size() - 4) == ".glb";
    if (glb)
        return loader.LoadBinaryFromMemory(&model, &err, &warn,
                   reinterpret_cast<const unsigned char*>(bytes.data()),
                   (unsigned int)bytes.size(), dir);
    std::string ascii = embedGltfExternals(bytes, dir);
    return loader.LoadASCIIFromString(&model, &err, &warn,
               ascii.data(), (unsigned int)ascii.size(), dir);
}

SkinnedMesh SkinnedMesh::load(const std::string& path)
{
    SkinnedMesh sm;

    {
        std::vector<float>        verts;
        std::vector<unsigned int> indices;
        if (tryLoadCache(path, sm, verts, indices)) {
            sm.upload(verts, indices);
            std::cout << "SkinnedMesh (cache) " << path
                      << "  v=" << verts.size()/16
                      << "  f=" << indices.size()/3
                      << "  joints=" << sm.invBindMats_.size() << "\n";
            for (auto& a : sm.animations_)
                std::cout << "  + anim '" << a.name << "' dur=" << a.duration << "s\n";
            return sm;
        }
    }

    tinygltf::Model model;
    std::string err, warn;
    bool ok = loadGltfModel(path, model, err, warn);

    if (!ok) throw GUIException("tinygltf: " + err);
    if (!warn.empty()) std::cerr << "tinygltf warn: " << warn << "\n";

    int numJoints = 0;
    if (!model.skins.empty()) {
        auto& skin = model.skins[0];
        numJoints  = (int)skin.joints.size();
        sm.parentOf_.resize(numJoints, -1);

        if (skin.inverseBindMatrices >= 0) {
            const float* ibm = floatPtr(model, skin.inverseBindMatrices);
            sm.invBindMats_.resize(numJoints);
            for (int i = 0; i < numJoints; ++i)
                memcpy(glm::value_ptr(sm.invBindMats_[i]), ibm + i*16, 64);
        } else {
            sm.invBindMats_.assign(numJoints, glm::mat4(1.f));
        }

        sm.boneNames_.resize(numJoints);
        sm.bindLocal_.resize(numJoints);
        for (int j = 0; j < numJoints; ++j) {
            int nodeIdx = skin.joints[j];
            const auto& nd = model.nodes[nodeIdx];
            LocalTRS& bl = sm.bindLocal_[j];
            if (nd.translation.size() == 3)
                bl.t = {(float)nd.translation[0],(float)nd.translation[1],(float)nd.translation[2]};
            if (nd.rotation.size() == 4)
                bl.r = {(float)nd.rotation[3],(float)nd.rotation[0],(float)nd.rotation[1],(float)nd.rotation[2]};
            if (nd.scale.size() == 3)
                bl.s = {(float)nd.scale[0],(float)nd.scale[1],(float)nd.scale[2]};
            sm.boneNames_[j] = model.nodes[nodeIdx].name;
            for (auto& child : model.nodes[nodeIdx].children) {
                auto it = std::find(skin.joints.begin(), skin.joints.end(), child);
                if (it != skin.joints.end())
                    sm.parentOf_[it - skin.joints.begin()] = j;
            }
        }

        {

            int numNodes = (int)model.nodes.size();
            std::vector<int> nodeParent(numNodes, -1);
            for (int ni = 0; ni < numNodes; ++ni)
                for (int child : model.nodes[ni].children)
                    if (child < numNodes) nodeParent[child] = ni;

            auto nodeMat = [&](int ni) -> glm::mat4 {
                auto& n = model.nodes[ni];
                if (!n.matrix.empty()) {
                    glm::mat4 m;
                    memcpy(glm::value_ptr(m), n.matrix.data(), 64);
                    return m;
                }
                glm::vec3 t(0.f); glm::quat r(1.f,0.f,0.f,0.f); glm::vec3 s(1.f);
                if (n.translation.size()==3) t = {(float)n.translation[0],(float)n.translation[1],(float)n.translation[2]};
                if (n.rotation.size()==4)    r = {(float)n.rotation[3],(float)n.rotation[0],(float)n.rotation[1],(float)n.rotation[2]};
                if (n.scale.size()==3)       s = {(float)n.scale[0],(float)n.scale[1],(float)n.scale[2]};
                return glm::translate(glm::mat4(1.f), t) * glm::toMat4(r) * glm::scale(glm::mat4(1.f), s);
            };

            int rootJointNode = skin.joints[0];

            std::vector<int> chain;
            for (int cur = nodeParent[rootJointNode]; cur >= 0; cur = nodeParent[cur])
                chain.push_back(cur);

            glm::mat4 xform(1.f);
            for (int i = (int)chain.size()-1; i >= 0; --i)
                xform = xform * nodeMat(chain[i]);
            sm.skinRootXform_ = xform;
        }
    }

    std::vector<float>        verts;
    std::vector<unsigned int> indices;

    for (auto& mesh : model.meshes) {
        for (auto& prim : mesh.primitives) {
            if (prim.mode != TINYGLTF_MODE_TRIANGLES) continue;

            auto posIt = prim.attributes.find("POSITION");
            auto nrmIt = prim.attributes.find("NORMAL");
            auto j0It  = prim.attributes.find("JOINTS_0");
            auto w0It  = prim.attributes.find("WEIGHTS_0");
            auto uvIt  = prim.attributes.find("TEXCOORD_0");
            if (posIt == prim.attributes.end()) continue;

            auto& posAcc = model.accessors[posIt->second];
            size_t nv = posAcc.count;
            const float* pos = floatPtr(model, posIt->second);
            const float* nrm = (nrmIt != prim.attributes.end())
                               ? floatPtr(model, nrmIt->second) : nullptr;
            const float* uv  = (uvIt  != prim.attributes.end())
                               ? floatPtr(model, uvIt->second)  : nullptr;

            std::vector<float> jvec(nv*4, 0.f), wvec(nv*4, 0.f);
            if (j0It != prim.attributes.end()) {
                auto& jAcc = model.accessors[j0It->second];
                if (jAcc.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE) {
                    auto& bv  = model.bufferViews[jAcc.bufferView];
                    auto& buf = model.buffers[bv.buffer];
                    const uint8_t* jb = buf.data.data() + bv.byteOffset + jAcc.byteOffset;
                    for (size_t i = 0; i < nv*4; ++i) jvec[i] = (float)jb[i];
                } else {
                    const uint16_t* ju = u16Ptr(model, j0It->second);
                    for (size_t i = 0; i < nv*4; ++i) jvec[i] = (float)ju[i];
                }
            }
            if (w0It != prim.attributes.end()) {
                const float* wf = floatPtr(model, w0It->second);
                for (size_t i = 0; i < nv*4; ++i) wvec[i] = wf[i];
            }

            if (j0It == prim.attributes.end() && w0It == prim.attributes.end()) {
                for (size_t i = 0; i < nv; ++i) wvec[i*4+0] = 1.f;
            }

            unsigned int base = (unsigned int)(verts.size() / 16);
            for (size_t i = 0; i < nv; ++i) {
                verts.push_back(pos[i*3+0]); verts.push_back(pos[i*3+1]); verts.push_back(pos[i*3+2]);
                if (nrm) { verts.push_back(nrm[i*3+0]); verts.push_back(nrm[i*3+1]); verts.push_back(nrm[i*3+2]); }
                else      { verts.push_back(0); verts.push_back(1); verts.push_back(0); }
                verts.push_back(jvec[i*4+0]); verts.push_back(jvec[i*4+1]);
                verts.push_back(jvec[i*4+2]); verts.push_back(jvec[i*4+3]);
                verts.push_back(wvec[i*4+0]); verts.push_back(wvec[i*4+1]);
                verts.push_back(wvec[i*4+2]); verts.push_back(wvec[i*4+3]);
                if (uv) { verts.push_back(uv[i*2+0]); verts.push_back(uv[i*2+1]); }
                else    { verts.push_back(0.f); verts.push_back(0.f); }

                sm.bmin_.x = std::min(sm.bmin_.x, pos[i*3+0]);
                sm.bmin_.y = std::min(sm.bmin_.y, pos[i*3+1]);
                sm.bmin_.z = std::min(sm.bmin_.z, pos[i*3+2]);
                sm.bmax_.x = std::max(sm.bmax_.x, pos[i*3+0]);
                sm.bmax_.y = std::max(sm.bmax_.y, pos[i*3+1]);
                sm.bmax_.z = std::max(sm.bmax_.z, pos[i*3+2]);
            }

            auto primIdx = getIndices(model, prim.indices);
            SubMesh sub;
            sub.start    = (GLsizei)indices.size();
            sub.material = prim.material;
            for (auto idx : primIdx) indices.push_back(base + idx);
            sub.count    = (GLsizei)indices.size() - sub.start;
            sm.subMeshes_.push_back(sub);
        }
    }

    std::vector<SubMeshMeta> smeta;
    std::map<int, std::string> imgSrcBytes;
    {
        smeta.reserve(sm.subMeshes_.size());
        for (auto& s : sm.subMeshes_) smeta.push_back({s.start, s.count, -1});

        int distinct = 0;
        std::map<int, GLuint> imgTex;
        std::vector<int> seen;
        for (auto& s : sm.subMeshes_) {
            if (s.material < 0 || s.material >= (int)model.materials.size()) continue;
            int ti = model.materials[s.material].pbrMetallicRoughness.baseColorTexture.index;
            if (ti < 0 || ti >= (int)model.textures.size()) continue;
            int src = model.textures[ti].source;
            if (std::find(seen.begin(), seen.end(), src) == seen.end()) { seen.push_back(src); ++distinct; }
        }
        if (distinct > 1) {
            for (size_t si = 0; si < sm.subMeshes_.size(); ++si) {
                auto& s = sm.subMeshes_[si];
                if (s.material < 0 || s.material >= (int)model.materials.size()) continue;
                int ti = model.materials[s.material].pbrMetallicRoughness.baseColorTexture.index;
                if (ti < 0 || ti >= (int)model.textures.size()) continue;
                int src = model.textures[ti].source;
                if (src < 0 || src >= (int)model.images.size()) continue;
                smeta[si].imgSrc = src;
                auto found = imgTex.find(src);
                if (found != imgTex.end()) { s.tex = found->second; continue; }

                std::string dir   = path.substr(0, path.find_last_of('/') + 1);
                std::string bytes = zappy::assetBytes(dir + "recolor_" + std::to_string(src) + ".png");
                if (bytes.empty()) {
                    auto& im = model.images[src];
                    if (im.bufferView >= 0 && im.bufferView < (int)model.bufferViews.size()) {
                        auto& bv  = model.bufferViews[im.bufferView];
                        auto& buf = model.buffers[bv.buffer];
                        bytes.assign((const char*)buf.data.data() + bv.byteOffset, bv.byteLength);
                    }
                }
                imgSrcBytes[src] = bytes;
                GLuint t = makeTexFromImageBytes(bytes);
                imgTex[src] = t;
                if (t) sm.matTextures_.push_back(t);
                s.tex = t;
            }
            std::cout << "  + " << distinct << " embedded material textures\n";
        }
    }

    std::cout << "SkinnedMesh " << path
              << "  v=" << verts.size()/16
              << "  f=" << indices.size()/3
              << "  joints=" << numJoints << "\n";

    if (!model.skins.empty()) {
        auto& skin = model.skins[0];
        for (auto& anim : model.animations) {
            Animation a;
            a.name = anim.name;
            a.duration = 0.f;

            for (auto& ch : anim.channels) {
                if (ch.target_node < 0) continue;

                auto it = std::find(skin.joints.begin(), skin.joints.end(), ch.target_node);
                if (it == skin.joints.end()) continue;
                int ji = (int)(it - skin.joints.begin());

                auto& samp = anim.samplers[ch.sampler];
                const float* times = floatPtr(model, samp.input);
                size_t       nk    = model.accessors[samp.input].count;
                const float* vals  = floatPtr(model, samp.output);

                BoneChannel* bc = nullptr;
                for (auto& c : a.channels)
                    if (c.boneIndex == ji) { bc = &c; break; }
                if (!bc) { a.channels.push_back({ji}); bc = &a.channels.back(); }

                if (nk > 0) a.duration = std::max(a.duration, times[nk-1]);

                if (ch.target_path == "translation") {
                    bc->timesT.assign(times, times+nk);
                    bc->transKeys.resize(nk);
                    for (size_t i = 0; i < nk; ++i)
                        bc->transKeys[i] = {vals[i*3], vals[i*3+1], vals[i*3+2]};
                } else if (ch.target_path == "rotation") {
                    bc->timesR.assign(times, times+nk);
                    bc->rotKeys.resize(nk);
                    for (size_t i = 0; i < nk; ++i)

                        bc->rotKeys[i] = glm::quat(vals[i*4+3], vals[i*4], vals[i*4+1], vals[i*4+2]);
                } else if (ch.target_path == "scale") {
                    bc->timesS.assign(times, times+nk);
                    bc->scaleKeys.resize(nk);
                    for (size_t i = 0; i < nk; ++i)
                        bc->scaleKeys[i] = {vals[i*3], vals[i*3+1], vals[i*3+2]};
                }
            }
            sm.animations_.push_back(std::move(a));
        }
    }

    sm.upload(verts, indices);
    saveCache(path, sm, verts, indices, smeta, imgSrcBytes);
    return sm;
}

void SkinnedMesh::loadAnimFrom(const std::string& path)
{
    {
        std::vector<Animation> loaded;
        if (tryLoadAnimOnlyCache(path, loaded)) {
            for (auto& a : loaded) {
                animations_.push_back(std::move(a));
                std::cout << "  + anim (cache) '" << animations_.back().name
                          << "' dur=" << animations_.back().duration << "s\n";
            }
            return;
        }
    }

    tinygltf::Model model;
    std::string err, warn;
    bool ok = loadGltfModel(path, model, err, warn);
    if (!ok) { std::cerr << "loadAnimFrom failed: " << err << "\n"; return; }

    if (model.skins.empty()) return;
    auto& skin = model.skins[0];

    for (auto& anim : model.animations) {
        Animation a;
        a.name = anim.name.empty() ? path : anim.name;
        a.duration = 0.f;

        for (auto& ch : anim.channels) {
            if (ch.target_node < 0) continue;
            auto it = std::find(skin.joints.begin(), skin.joints.end(), ch.target_node);
            if (it == skin.joints.end()) continue;
            int ji = (int)(it - skin.joints.begin());

            auto& samp = anim.samplers[ch.sampler];
            const float* times = floatPtr(model, samp.input);
            size_t       nk    = model.accessors[samp.input].count;
            const float* vals  = floatPtr(model, samp.output);

            BoneChannel* bc = nullptr;
            for (auto& c : a.channels)
                if (c.boneIndex == ji) { bc = &c; break; }
            if (!bc) { a.channels.push_back({ji}); bc = &a.channels.back(); }

            if (nk > 0) a.duration = std::max(a.duration, times[nk-1]);

            if (ch.target_path == "translation") {
                bc->timesT.assign(times, times+nk);
                bc->transKeys.resize(nk);
                for (size_t i = 0; i < nk; ++i)
                    bc->transKeys[i] = {vals[i*3], vals[i*3+1], vals[i*3+2]};
            } else if (ch.target_path == "rotation") {
                bc->timesR.assign(times, times+nk);
                bc->rotKeys.resize(nk);
                for (size_t i = 0; i < nk; ++i)
                    bc->rotKeys[i] = glm::quat(vals[i*4+3], vals[i*4], vals[i*4+1], vals[i*4+2]);
            } else if (ch.target_path == "scale") {
                bc->timesS.assign(times, times+nk);
                bc->scaleKeys.resize(nk);
                for (size_t i = 0; i < nk; ++i)
                    bc->scaleKeys[i] = {vals[i*3], vals[i*3+1], vals[i*3+2]};
            }
        }
        animations_.push_back(std::move(a));
        std::cout << "  + anim '" << animations_.back().name
                  << "' dur=" << animations_.back().duration << "s\n";
    }

    std::vector<Animation> newAnims(animations_.end() - (int)model.animations.size(), animations_.end());
    saveAnimCache(path, newAnims);
}

void SkinnedMesh::computeBones(int animIdx, float t,
                               glm::mat4 boneMats[MAX_BONES],
                               bool loop, bool reversed,
                               bool stripRootMotion) const
{
    int n = (int)invBindMats_.size();
    if (n == 0) {
        for (int i = 0; i < MAX_BONES; ++i) boneMats[i] = glm::mat4(1.f);
        return;
    }

    if (n > MAX_BONES) n = MAX_BONES;

    std::vector<glm::mat4> local(n, glm::mat4(1.f));
    for (int i = 0; i < n && i < (int)bindLocal_.size(); ++i)
        local[i] = glm::translate(glm::mat4(1.f), bindLocal_[i].t)
                 * glm::toMat4(bindLocal_[i].r)
                 * glm::scale(glm::mat4(1.f), bindLocal_[i].s);

    if (animIdx >= 0 && animIdx < (int)animations_.size()) {
        auto& anim = animations_[animIdx];
        float dur = anim.duration;
        if (dur > 0.f) {
            if (loop)    t = std::fmod(t, dur);
            else         t = std::min(t, dur);
            if (reversed) t = dur - t;
        }

        int rootMoJoint = -1;
        if (stripRootMotion) {
            int bestDepth = 1 << 30;
            for (auto& ch : anim.channels) {
                if (ch.transKeys.empty()) continue;
                int ji = ch.boneIndex;
                if (ji >= n) continue;
                if (glm::length(ch.transKeys.back() - ch.transKeys.front()) < 0.05f)
                    continue;
                int d = 0, p = ji;
                while (p >= 0 && p < (int)parentOf_.size()) { p = parentOf_[p]; ++d; }
                if (d < bestDepth) { bestDepth = d; rootMoJoint = ji; }
            }
        }

        for (auto& ch : anim.channels) {
            int ji = ch.boneIndex;
            if (ji >= n) continue;

            glm::vec3 T = (ji < (int)bindLocal_.size()) ? bindLocal_[ji].t : glm::vec3(0.f);
            glm::quat R = (ji < (int)bindLocal_.size()) ? bindLocal_[ji].r : glm::quat(1.f,0.f,0.f,0.f);
            glm::vec3 S = (ji < (int)bindLocal_.size()) ? bindLocal_[ji].s : glm::vec3(1.f);

            if (!ch.transKeys.empty())  T = sampleLinear(ch.timesT, ch.transKeys, t);
            if (!ch.rotKeys.empty())    R = sampleSlerp (ch.timesR, ch.rotKeys,   t);
            if (!ch.scaleKeys.empty())  S = sampleLinear(ch.timesS, ch.scaleKeys, t);

            if (stripRootMotion && ji == rootMoJoint && !ch.transKeys.empty())
                T = ch.transKeys.front();

            glm::mat4 m = glm::translate(glm::mat4(1.f), T)
                        * glm::toMat4(R)
                        * glm::scale(glm::mat4(1.f), S);
            local[ji] = m;
        }
    }

    std::vector<glm::mat4> global(n);
    for (int i = 0; i < n; ++i) {
        int p = parentOf_[i];
        if (p >= 0 && p < n)
            global[i] = global[p] * local[i];
        else
            global[i] = local[i];
    }

    for (int i = 0; i < n; ++i)
        boneMats[i] = global[i] * invBindMats_[i];

    for (int i = n; i < MAX_BONES; ++i)
        boneMats[i] = glm::mat4(1.f);
}

void SkinnedMesh::upload(const std::vector<float>& verts,
                         const std::vector<unsigned int>& idx)
{
    indexCount_ = (GLsizei)idx.size();
    glGenVertexArrays(1, &vao_);
    glGenBuffers(1, &vbo_);
    glGenBuffers(1, &ebo_);

    glBindVertexArray(vao_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferData(GL_ARRAY_BUFFER, verts.size()*4, verts.data(), GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo_);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, idx.size()*4, idx.data(), GL_STATIC_DRAW);

    const GLsizei stride = 16 * sizeof(float);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride, (void*)(3*4));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, stride, (void*)(6*4));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, stride, (void*)(10*4));
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(4, 2, GL_FLOAT, GL_FALSE, stride, (void*)(14*4));
    glEnableVertexAttribArray(4);

    glBindVertexArray(0);
}

void SkinnedMesh::loadVertexColors(const std::string& colPath)
{
    std::string blob = zappy::assetBytes(colPath);
    if (blob.size() < 4) { std::cerr << "loadVertexColors: cannot open " << colPath << "\n"; return; }

    unsigned int n = 0;
    std::memcpy(&n, blob.data(), 4);
    std::vector<float> colors(n * 3);
    size_t need = (size_t)n * 3 * sizeof(float);
    if (blob.size() >= 4 + need)
        std::memcpy(colors.data(), blob.data() + 4, need);

    glBindVertexArray(vao_);
    if (!colorVbo_) glGenBuffers(1, &colorVbo_);
    glBindBuffer(GL_ARRAY_BUFFER, colorVbo_);
    glBufferData(GL_ARRAY_BUFFER, colors.size()*4, colors.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(4, 3, GL_FLOAT, GL_FALSE, 3*sizeof(float), (void*)0);
    glEnableVertexAttribArray(4);
    glBindVertexArray(0);
    std::cout << "Loaded vertex colors: " << n << " verts from " << colPath << "\n";
}

static GLuint makeTexFromImageBytes(const std::string& bytes)
{
    sf::Image img;
    if (bytes.empty() ||
        !img.loadFromMemory(bytes.data(), bytes.size()))
        return 0;
    auto sz = img.getSize();
    GLuint t = 0;
    glGenTextures(1, &t);
    glBindTexture(GL_TEXTURE_2D, t);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, sz.x, sz.y, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, img.getPixelsPtr());
    glGenerateMipmap(GL_TEXTURE_2D);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glBindTexture(GL_TEXTURE_2D, 0);
    return t;
}

void SkinnedMesh::loadTexture(const std::string& pngPath)
{
    std::string png = zappy::assetBytes(pngPath);
    sf::Image img;
    if (png.empty() || !img.loadFromMemory(png.data(), png.size())) {
        std::cerr << "loadTexture: cannot open " << pngPath << "\n";
        return;
    }
    auto sz = img.getSize();

    if (!texId_) glGenTextures(1, &texId_);
    glBindTexture(GL_TEXTURE_2D, texId_);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, sz.x, sz.y, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, img.getPixelsPtr());
    glGenerateMipmap(GL_TEXTURE_2D);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glBindTexture(GL_TEXTURE_2D, 0);
    std::cout << "Loaded texture " << sz.x << "x" << sz.y << " from " << pngPath << "\n";
}

void SkinnedMesh::draw() const
{
    glBindVertexArray(vao_);
    glActiveTexture(GL_TEXTURE0);

    bool multi = !matTextures_.empty() && texId_ == 0;
    if (multi) {
        for (auto& s : subMeshes_) {
            glBindTexture(GL_TEXTURE_2D, s.tex);
            glDrawElements(GL_TRIANGLES, s.count, GL_UNSIGNED_INT,
                           (const void*)(intptr_t)(s.start * sizeof(unsigned int)));
        }
    } else {
        if (texId_) glBindTexture(GL_TEXTURE_2D, texId_);
        glDrawElements(GL_TRIANGLES, indexCount_, GL_UNSIGNED_INT, nullptr);
    }

    glBindVertexArray(0);
    glBindTexture(GL_TEXTURE_2D, 0);
}

int SkinnedMesh::boneIndexByName(const std::string& n) const
{
    for (int i = 0; i < (int)boneNames_.size(); ++i)
        if (boneNames_[i] == n) return i;
    return -1;
}

void SkinnedMesh::addAnimation(Animation a)
{
    animations_.push_back(std::move(a));
}

void SkinnedMesh::overlayBoneChain(int rootBoneIdx, const glm::mat4& localRot,
                                    glm::mat4 boneMats[MAX_BONES]) const
{
    int n = (int)invBindMats_.size();
    if (rootBoneIdx < 0 || rootBoneIdx >= n) return;

    glm::mat4 bind_root = glm::inverse(invBindMats_[rootBoneIdx]);
    glm::mat4 global_root = boneMats[rootBoneIdx] * bind_root;
    glm::mat4 W = global_root * localRot * glm::inverse(global_root);

    for (int j = 0; j < n; ++j) {
        bool isDesc = (j == rootBoneIdx);
        if (!isDesc) {
            int p = parentOf_[j];
            while (p >= 0) {
                if (p == rootBoneIdx) { isDesc = true; break; }
                p = parentOf_[p];
            }
        }
        if (isDesc) boneMats[j] = W * boneMats[j];
    }
}

std::vector<LocalTRS> SkinnedMesh::extractLocalTRS(int animIdx, float t) const
{
    int n = (int)invBindMats_.size();
    std::vector<LocalTRS> out(n);

    if (animIdx < 0 || animIdx >= (int)animations_.size()) return out;
    auto& anim = animations_[animIdx];
    float dur = anim.duration;
    if (dur > 0.f) t = std::fmod(t, dur);

    for (auto& ch : anim.channels) {
        int ji = ch.boneIndex;
        if (ji < 0 || ji >= n) continue;
        if (!ch.transKeys.empty()) out[ji].t = sampleLinear(ch.timesT, ch.transKeys, t);
        if (!ch.rotKeys.empty())   out[ji].r = sampleSlerp (ch.timesR, ch.rotKeys,   t);
        if (!ch.scaleKeys.empty()) out[ji].s = sampleLinear(ch.timesS, ch.scaleKeys, t);
    }
    return out;
}

SkinnedMesh::~SkinnedMesh()
{
    if (vao_)     { glDeleteVertexArrays(1, &vao_); glDeleteBuffers(1, &vbo_); glDeleteBuffers(1, &ebo_); }
    if (colorVbo_) glDeleteBuffers(1, &colorVbo_);
    if (texId_)    glDeleteTextures(1, &texId_);
    for (GLuint t : matTextures_) glDeleteTextures(1, &t);
}

SkinnedMesh::SkinnedMesh(SkinnedMesh&& o) noexcept
    : vao_(o.vao_), vbo_(o.vbo_), ebo_(o.ebo_), colorVbo_(o.colorVbo_),
      texId_(o.texId_), indexCount_(o.indexCount_),
      invBindMats_(std::move(o.invBindMats_)),
      parentOf_(std::move(o.parentOf_)),
      bindLocal_(std::move(o.bindLocal_)),
      boneNames_(std::move(o.boneNames_)),
      skinRootXform_(o.skinRootXform_),
      animations_(std::move(o.animations_)),
      subMeshes_(std::move(o.subMeshes_)),
      matTextures_(std::move(o.matTextures_)),
      bmin_(o.bmin_), bmax_(o.bmax_)
{ o.vao_ = o.vbo_ = o.ebo_ = o.colorVbo_ = o.texId_ = 0; o.matTextures_.clear(); }

SkinnedMesh& SkinnedMesh::operator=(SkinnedMesh&& o) noexcept
{
    if (this != &o) {
        this->~SkinnedMesh();
        new(this) SkinnedMesh(std::move(o));
    }
    return *this;
}
