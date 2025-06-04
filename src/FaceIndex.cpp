// FaceIndex.cpp

#include "FaceIndex.hpp"
#include <cmath> // for sqrt
#include <fstream>
#include <sstream>

// Constructor: create L2Space and hnswlib index (max 10,000 entries for now)
FaceIndex::FaceIndex(int dim)
    : dim(dim)
{
    space = std::make_unique<hnswlib::L2Space>(dim);
    index = std::make_unique<hnswlib::HierarchicalNSW<float>>(space.get(), 10000);
}

// Normalize a vector to unit length (so L2 equals cosine distance for faces)
std::vector<float> FaceIndex::normalize(const std::vector<float>& v) {
    float norm = 0.0f;
    for (float x : v) norm += x*x;
    norm = std::sqrt(norm);
    std::vector<float> out(v.size());
    if (norm > 0) {
        for (size_t i = 0; i < v.size(); ++i)
            out[i] = v[i] / norm;
    }
    return out;
}

// Add a name and embedding to the index (embeddings always normalized)
void FaceIndex::add(const std::string& name, const std::vector<float>& embedding)
{
    std::vector<float> normed = normalize(embedding);
    index->addPoint(normed.data(), nextId);
    idToName[nextId] = name;
    ++nextId;
}

// Search for closest face. Returns {name, cosine_sim}, or {"", 0.0f} if no good match.
std::pair<std::string, float> FaceIndex::search(const std::vector<float>& embedding, float threshold)
{
    std::vector<float> normed = normalize(embedding);
    auto result = index->searchKnn(normed.data(), 1); // top-1 neighbor
    if (result.empty()) return {"", 0.0f};
    auto item = result.top();
    size_t id = item.second;
    float l2 = item.first; // L2 distance on unit vectors (range: 0=identical, 2=opposite)
    // Convert L2 to cosine similarity: cosine_sim = 1 - (l2^2)/2
    float cosine_sim = 1.0f - (l2 * l2) / 2.0f;
    if (cosine_sim < threshold) return {"", cosine_sim};
    return {idToName[id], cosine_sim};
}

void FaceIndex::saveToDisk(const std::string& path) {
    std::ofstream out(path);
    if (!out) return;
    for (const auto& pair : idToName) {
        size_t id = pair.first;
        const std::string& name = pair.second;
        // Get internal id from label_lookup_
        auto search = index->label_lookup_.find(id);
        if (search == index->label_lookup_.end()) continue;
        hnswlib::tableint internal_id = search->second;
        // Get embedding pointer
        float* emb_ptr = reinterpret_cast<float*>(index->getDataByInternalId(internal_id));
        std::vector<float> emb(emb_ptr, emb_ptr + dim);
        out << name;
        for (float v : emb) out << ',' << v;
        out << '\n';
    }
}

void FaceIndex::loadFromDisk(const std::string& path) {
    std::ifstream in(path);
    if (!in) return;
    // Clear current index
    space = std::make_unique<hnswlib::L2Space>(dim);
    index = std::make_unique<hnswlib::HierarchicalNSW<float>>(space.get(), 10000);
    idToName.clear();
    nextId = 0;
    std::string line;
    while (std::getline(in, line)) {
        std::istringstream ss(line);
        std::string name;
        if (!std::getline(ss, name, ',')) continue;
        std::vector<float> emb(dim);
        for (int i = 0; i < dim; ++i) {
            std::string val;
            if (!std::getline(ss, val, ',')) break;
            emb[i] = std::stof(val);
        }
        add(name, emb);
    }
}
