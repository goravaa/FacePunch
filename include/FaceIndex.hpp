// FaceIndex.hpp
#pragma once

#include <vector>
#include <string>
#include <unordered_map>
#include <memory>
#include "hnswlib/hnswlib.h"
#include "hnswlib/space_l2.h"

// FaceIndex: Stores embeddings and lets you do fast nearest-neighbor face search using hnswlib
class FaceIndex {
public:
    // Constructor: sets up hnswlib index for L2 distance with fixed dimension
    FaceIndex(int dim);

    // Add a (name, embedding) pair to the index
    void add(const std::string& name, const std::vector<float>& embedding);

    // Search for the most similar face. Returns {name, similarity}
    // Similarity is cosine-like (higher is more similar, max 1.0)
    std::pair<std::string, float> search(const std::vector<float>& embedding, float threshold = 0.7);

    // Save all embeddings and names to disk (CSV format)
    void saveToDisk(const std::string& path);
    // Load all embeddings and names from disk (CSV format)
    void loadFromDisk(const std::string& path);

private:
    int dim; // dimension of each embedding
    size_t nextId = 0; // unique integer label for hnswlib
    std::unordered_map<size_t, std::string> idToName; // map hnswlib labels to user names

    std::unique_ptr<hnswlib::L2Space> space;
    std::unique_ptr<hnswlib::HierarchicalNSW<float>> index;

    // Helper to normalize a vector to length 1
    std::vector<float> normalize(const std::vector<float>& v);
};
