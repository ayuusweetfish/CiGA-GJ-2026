#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <bitset>
#include <chrono>
#include <random>
#include <algorithm>

using namespace std;

// --------------------------------------
// Split a string by a delimiter
vector<string> split(const string &s, char delim) {
  vector<string> tokens;
  string token;
  istringstream tokenStream(s);
  while (getline(tokenStream, token, delim))
    tokens.push_back(token);
  return tokens;
}

struct pair_hash {
  template <typename T1, typename T2>
  std::size_t operator () (const std::pair<T1, T2> &p) const {
    auto h1 = std::hash<T1>{}(p.first);
    auto h2 = std::hash<T2>{}(p.second);
    return h1 ^ (h2 + 0x9e3779b9 + (h1 << 6) + (h1 >> 2));
  }
};

string trim_end(const string &s) {
  size_t end = s.find_last_not_of("\n\r\t\f\v");
  return s.substr(0, end != std::string::npos ? (end + 1) : 0);
}

// trim both ends (including space)
string trim(const string &s) {
  size_t start = s.find_first_not_of(" \t\n\r\f\v");
  if (start == string::npos) return "";
  size_t end = s.find_last_not_of(" \t\n\r\f\v");
  return s.substr(start, end - start + 1);
}

// --------------------------------------
int main(int argc, char *argv[]) {
  string bbox_file_name;
  string labeldesc_file_name;
  int n_walks;

  if (argc < 2) {
    cerr << "Usage: " << argv[0] << " <n_walks>" << endl;
    return 1;
  }

  bbox_file_name =
#if 0
    "/tmp/bbox-part.csv";
#else
    "oidv6-train-annotations-bbox.csv";
#endif
  labeldesc_file_name = "oidv7-class-descriptions-boxable.csv";
  n_walks = stoi(argv[1]);

  // Random engine for walks
  random_device rd;
  mt19937 rng(rd());

  // ---------- 1. Read class display names ----------
  unordered_map<string, string> label_display_name;
  {
    ifstream labeldesc_file(labeldesc_file_name);
    if (!labeldesc_file.is_open()) {
      cerr << "Cannot open " << labeldesc_file_name << endl;
      return 1;
    }
    string line;
    getline(labeldesc_file, line);  // Skip header
    while (getline(labeldesc_file, line)) {
      line = trim_end(line);
      if (line.empty()) continue;
      auto cols = split(line, ',');
      if (cols.size() >= 2)
        label_display_name[cols[0]] = cols[1];
    }
  }

  // ---------- 2. Read annotations ----------
  ifstream file(bbox_file_name);
  if (!file.is_open()) {
    cerr << "Cannot open " << bbox_file_name << endl;
    return 1;
  }

  cout << "Reading bounding box annotations from " << bbox_file_name << endl;

  unordered_map<string, int> image_index;
  unordered_map<string, int> label_index;
  vector<string> image_ids;
  vector<string> label_names;
  vector<unordered_set<int>> img_adj;   // temporary sets
  vector<unordered_set<int>> lbl_adj;

  struct bbox {
    float xmin, xmax, ymin, ymax;
  };
  unordered_map<pair<int, int>, bbox, pair_hash> img_lbl_bbox;

  const int MAX_N_LABELS = 600;
  vector<bitset<MAX_N_LABELS>> img_labels;

  string line;
  getline(file, line); // skip header
  while (getline(file, line)) {
    if (line.empty()) continue;
    auto cols = split(line, ',');
    if (cols.size() < 3) continue;    // need at least ImageID and LabelName
    string img_id = cols[0];
    string lbl_name = cols[2];
    bbox box = {
      .xmin = stof(cols[4]),
      .xmax = stof(cols[5]),
      .ymin = stof(cols[6]),
      .ymax = stof(cols[7]),
    };

    // Map image ID
    int img_idx;
    auto it_img = image_index.find(img_id);
    if (it_img == image_index.end()) {
      img_idx = image_ids.size();
      image_index[img_id] = img_idx;
      image_ids.push_back(img_id);
      img_adj.emplace_back();
      img_labels.emplace_back();
    } else {
      img_idx = it_img->second;
    }

    // Map label name
    int lbl_idx;
    auto it_lbl = label_index.find(lbl_name);
    if (it_lbl == label_index.end()) {
      lbl_idx = label_names.size();
      label_index[lbl_name] = lbl_idx;
      label_names.push_back(lbl_name);
      lbl_adj.emplace_back();
    } else {
      lbl_idx = it_lbl->second;
    }

    // Add edge (both sides)
    if (
      (box.xmax - box.xmin) * (box.ymax - box.ymin) < 0.1 &&
      label_display_name[label_names[lbl_idx]].find("Human") == std::string::npos
    ) {
      img_adj[img_idx].insert(lbl_idx);
      lbl_adj[lbl_idx].insert(img_idx);
    }

    // Record in bitset
    img_labels[img_idx].set(lbl_idx, true);
    // Record bounding box
    img_lbl_bbox[make_pair(img_idx, lbl_idx)] = box;
  }
  file.close();

  int I = image_ids.size();
  int L = label_names.size();
  int V = I + L;
  cout << "Images: " << I << ", Labels: " << L << ", Total vertices: " << V << endl;

  // ---------- 3. Build adjacency lists ----------
  // Convert sets to vectors for speed
  vector<vector<int>> img_adj_vec(I);
  for (int i = 0; i < I; ++i) {
    img_adj_vec[i].assign(img_adj[i].begin(), img_adj[i].end());
    sort(img_adj_vec[i].begin(), img_adj_vec[i].end());
  }
  vector<vector<int>> lbl_adj_vec(L);
  for (int j = 0; j < L; ++j) {
    lbl_adj_vec[j].assign(lbl_adj[j].begin(), lbl_adj[j].end());
    sort(lbl_adj_vec[j].begin(), lbl_adj_vec[j].end());
  }
  // Free temporary sets
  img_adj.clear(); img_adj.shrink_to_fit();
  lbl_adj.clear(); lbl_adj.shrink_to_fit();

  // Unified graph: [0..I-1] images, [I..I+L-1] labels
  vector<vector<int>> graph(V);
  for (int i = 0; i < I; ++i) {
    for (int lbl : img_adj_vec[i]) {
      int v = I + lbl;
      graph[i].push_back(v);
      graph[v].push_back(i);
    }
  }

  // ---------- 4. Build display name -> label index mapping ----------
  unordered_map<string, vector<int>> display_to_labels;
  for (int i = 0; i < L; ++i) {
    auto it = label_display_name.find(label_names[i]);
    if (it != label_display_name.end()) {
      display_to_labels[it->second].push_back(i);
    }
  }

  // ---------- 5. Query loop (random walks) ----------
  cerr << "Ready for queries. Enter 'DisplayName1, DisplayName2' per line." << endl;
  string query_line;
  while (getline(cin, query_line)) {
    query_line = trim(query_line);
    if (query_line.empty()) continue;

    // Split by first comma
    size_t comma_pos = query_line.find(',');
    if (comma_pos == string::npos) {
      cerr << "Invalid format, expected 'Name1, Name2'" << endl;
      continue;
    }
    string name1 = trim(query_line.substr(0, comma_pos));
    string name2 = trim(query_line.substr(comma_pos + 1));

    auto it1 = display_to_labels.find(name1);
    auto it2 = display_to_labels.find(name2);
    if (it1 == display_to_labels.end() || it1->second.empty()) {
      cout << "Unknown label: " << name1 << endl;
      continue;
    }
    if (it2 == display_to_labels.end() || it2->second.empty()) {
      cout << "Unknown label: " << name2 << endl;
      continue;
    }

    // For simplicity take the first matching label index.
    // (If multiple labels share the same display name, we could try all; 
    //  here we just pick the first.)
    int start_label = it1->second[0];
    int target_label = it2->second[0];

    // If start == target, just output a single image containing it.
    if (start_label == target_label) {
      if (lbl_adj_vec[start_label].empty()) {
        cout << "No chain found (label has no images)." << endl;
        continue;
      }
      int img = lbl_adj_vec[start_label][0];
      bbox box = img_lbl_bbox.at({img, start_label});
      string disp = label_display_name.at(label_names[start_label]);
      cout << fixed << setprecision(4);
      cout << box.xmin << " " << box.xmax << " " << box.ymin << " " << box.ymax
           << " -> " << image_ids[img] << " -> "
           << box.xmin << " " << box.xmax << " " << box.ymin << " " << box.ymax
           << " | " << disp << endl;
      continue;
    }

    bool found = false;
    uniform_int_distribution<size_t> dist;

    // Perform n_walks random walks
    for (int walk = 0; walk < n_walks && !found; ++walk) {
      // Random starting image containing start_label
      const auto &start_imgs = lbl_adj_vec[start_label];
      if (start_imgs.empty()) continue;
      int cur_img = start_imgs[rng() % start_imgs.size()];

      vector<int> img_seq = {cur_img};
      vector<int> lbl_seq;   // labels connecting images

      bool dead_end = false;
      const int MAX_STEPS = 50;
      for (int step = 0; step < MAX_STEPS; ++step) {
        // Check if target reached
        if (img_labels[cur_img].test(target_label)) {
          found = true;
          break;
        }

        // Gather candidate (label, next_image) pairs
        vector<pair<int, int>> candidates;
        for (int lbl : img_adj_vec[cur_img]) {
          const auto &imgs = lbl_adj_vec[lbl];
          if (imgs.size() <= 1) continue; // only the current image itself
          // Try up to 100 random images from this label
          int attempts = min<int>(100, imgs.size());
          for (int a = 0; a < attempts; ++a) {
            int nxt = imgs[rng() % imgs.size()];
            if (nxt == cur_img) continue;
            // Check that the two images share at most 2 labels
            if ((img_labels[cur_img] & img_labels[nxt]).count() <= 2) {
              candidates.push_back({lbl, nxt});
              break;
            }
          }
        }

        if (candidates.empty()) {
          dead_end = true;
          break;
        }

        // Pick random candidate
        auto [lbl, nxt] = candidates[rng() % candidates.size()];
        lbl_seq.push_back(lbl);
        cur_img = nxt;
        img_seq.push_back(cur_img);
      }

      if (found) {
        // Print the chain
        int n = img_seq.size();
        const auto label = [&] (int i) -> int {
          if (i < 0) return start_label;
          if (i >= n - 1) return target_label;
          return lbl_seq[i];
        };
        for (int i = -1; i <= n; ++i) {
          int img = img_seq[i < 0 ? 0 : i >= n ? n - 1 : i];
          int in_lbl = label(i - 1);
          int out_lbl = label(i);

          bbox b1 = img_lbl_bbox.at({img, in_lbl});
          bbox b2 = img_lbl_bbox.at({img, out_lbl});
          if (i < 0) b1 = (bbox){0};
          string img_id_str = image_ids[img];
          string out_disp = label_display_name.at(label_names[out_lbl]);

          cout << fixed << setprecision(4);
          cout << b1.xmin << " " << b1.xmax << " " << b1.ymin << " " << b1.ymax
            << " -> " << img_id_str;
          if (i < n)
            cout << " -> "
              << b2.xmin << " " << b2.xmax << " " << b2.ymin << " " << b2.ymax
              << " | " << out_disp;
          cout << endl;
        }
      }
    }

    if (!found) {
      cout << "No chain found between '" << name1 << "' and '" << name2 << "'." << endl;
    }
  }

  return 0;
}
