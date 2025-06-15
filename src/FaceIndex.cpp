// FaceIndex.cpp

#include "FaceIndex.hpp"
#include <cmath> // for sqrt
#include <fstream>
#include <sstream>
#include <QDebug> // For qWarning()

// Constructor: create L2Space and hnswlib index
FaceIndex::FaceIndex(int dim, int max_elements)
    : dim(dim), max_elements_(max_elements) // Initialize max_elements_
{
    space = std::make_unique<hnswlib::L2Space>(dim);
    index = std::make_unique<hnswlib::HierarchicalNSW<float>>(space.get(), max_elements_); // Use max_elements_
}

// Add a name and embedding to the index (embeddings always normalized)
void FaceIndex::add(const std::string& name, const std::vector<float>& embedding)
{
    // Embeddings are assumed to be pre-normalized
    index->addPoint(embedding.data(), nextId);
    idToName[nextId] = name;
    ++nextId;
}

// Search for closest face. Returns a SearchResult struct.
SearchResult FaceIndex::search(const std::vector<float>& embedding, float threshold)
{
    // Embeddings are assumed to be pre-normalized
    auto result_queue = index->searchKnn(embedding.data(), 1); // top-1 neighbor
    if (result_queue.empty()) {
        return {"", 0.0f, 0, false};
    }
    auto item = result_queue.top();
    size_t found_id = item.second;
    float l2_distance = item.first; // L2 distance on unit vectors (range: 0=identical, 2=opposite)

    // Convert L2 to cosine similarity: cosine_sim = 1 - (l2^2)/2
    float cosine_sim = 1.0f - (l2_distance * l2_distance) / 2.0f;

    if (cosine_sim < threshold) {
        // Similarity below threshold, but we can still return what was found if needed for context
        // For attendance logging, we only care about confirmed matches above threshold.
        return {"", cosine_sim, 0, false}; // Or: {idToName[found_id], cosine_sim, found_id, false} if you want to know who was close but below threshold
    }

    auto it = idToName.find(found_id);
    if (it == idToName.end()) {
        // This case should ideally not happen if HNSW index and idToName are perfectly synced.
        // Could occur if an ID was deleted from idToName but not perfectly from HNSW.
        qWarning() << "HNSW index returned ID" << found_id << "but it's not in idToName map.";
        return {"", cosine_sim, found_id, false}; // Indicate inconsistency
    }

    return {it->second, cosine_sim, found_id, true};
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
    index = std::make_unique<hnswlib::HierarchicalNSW<float>>(space.get(), max_elements_); // Use max_elements_
    idToName.clear();
    nextId = 0;
    index = std::make_unique<hnswlib::HierarchicalNSW<float>>(space.get(), max_elements_); // Ensure index is fresh before load attempt

    std::string line;
    try {
        while (std::getline(in, line)) {
            std::istringstream ss(line);
            std::string name;
            if (!std::getline(ss, name, ',')) continue;
            std::vector<float> emb(dim);
            bool parse_error = false;
            for (int i = 0; i < dim; ++i) {
                std::string val;
                if (!std::getline(ss, val, ',')) {
                    parse_error = true;
                    break;
                }
                try {
                    emb[i] = std::stof(val);
                } catch (const std::invalid_argument& ia) {
                    qWarning() << "Invalid number format in database line:" << QString::fromStdString(line) << "value:" << QString::fromStdString(val);
                    parse_error = true;
                    break;
                } catch (const std::out_of_range& oor) {
                    qWarning() << "Number out of range in database line:" << QString::fromStdString(line) << "value:" << QString::fromStdString(val);
                    parse_error = true;
                    break;
                }
            }
            if (!parse_error && emb.size() == static_cast<size_t>(dim)) { // ensure enough values were read
                add(name, emb);
            } else if (parse_error) {
                 qWarning() << "Skipping corrupted or incomplete line in face database:" << QString::fromStdString(line);
            }
        }
    } catch (const std::exception& e) {
        qWarning() << "Error parsing face database file" << QString::fromStdString(path) << ":" << e.what();
        // Clear potentially partially loaded data and reset index
        idToName.clear();
        index = std::make_unique<hnswlib::HierarchicalNSW<float>>(space.get(), max_elements_);
        nextId = 0;
    }
}

const std::unordered_map<size_t, std::string>& FaceIndex::getIdToNameMap() const {
    return idToName;
}

bool FaceIndex::deleteUser(size_t label) {
    if (idToName.find(label) == idToName.end()) {
        qWarning() << "Attempted to delete non-existent user with label:" << label;
        return false; // Label not found in our map
    }

    try {
        // HNSWlib uses 'label' as the external label passed to addPoint
        index->markDelete(label);
    } catch (const std::runtime_error& e) {
        // This exception can be thrown if the label is not found in the HNSW graph,
        // or if the element was already deleted, or other HNSW internal issues.
        qWarning() << "Failed to mark label" << label << "as deleted in HNSW index:" << e.what();
        // Depending on strictness, could return false. If we proceed, the user is removed
        // from idToName map, so they won't be searchable or re-savable with this ID.
        // If HNSW failed but it was in idToName, it's an inconsistency.
        // For robustness, let's ensure it's removed from our map anyway.
    }

    idToName.erase(label);
    // Note: nextId is NOT decremented. New users will get fresh IDs.
    // HNSWlib handles reuse of space internally if elements are re-added later,
    // but we are not re-using labels with current 'nextId++' logic.
    return true;
}

bool FaceIndex::updateUserName(size_t label, const std::string& newName) {
    auto it = idToName.find(label);
    if (it == idToName.end()) {
        qWarning() << "Attempted to update name for non-existent user with label:" << label;
        return false; // Label not found
    }
    if (newName.empty()) {
        qWarning() << "Attempted to update user" << label << "with an empty name.";
        return false; // Or handle as an error, prevent empty names
    }
    it->second = newName;
    return true;
}
