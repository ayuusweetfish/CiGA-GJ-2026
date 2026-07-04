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

  // ---------- 1. Read annotations ----------
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
    if ((box.xmax - box.xmin) * (box.ymax - box.ymin) < 0.1) {
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

  // ---------- 2. Read class display names ----------
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

  // ---------- 3. Random traversal statistics ----------
  cout << "\n=== Random Traversal Statistics ===\n";

  auto t_start = chrono::steady_clock::now();
  int last_print_elapsed = 0;

  const int MAX_WALK_LEN = 200;

  // Simple LCG
  auto rand_int = [] (unsigned &seed, int max_val) -> int {
    seed = (seed * 1103515245u + 12345u) & 0x7fffffffu;
    return seed % max_val;
  };

  unsigned rng_seed = 42;
  vector<int> walk_lengths(MAX_WALK_LEN, 0);
  vector<int> checkpoint_longest;

  auto print_stats = [I, &image_ids, &label_names, &label_display_name,
    &walk_lengths, &checkpoint_longest, &img_lbl_bbox] (int n_walks) -> void
  {
    cout << "Walk length distribution (from " << n_walks << " random walks):\n";
    cout << "Length\tCount\n";
    for (int i = 0; i < walk_lengths.size(); i++) if (walk_lengths[i] != 0) {
      cout << i << "\t" << walk_lengths[i] << "\n";
    }
    cout << "Checkpoint longest (length " << checkpoint_longest.size() << "):\n";
    int last_image = -1, last_label = -1;
    for (int u : checkpoint_longest) {
      if (u < I) {
        auto box = img_lbl_bbox[make_pair(u, last_label)];
        cout
          << fixed << setprecision(4) << box.xmin << " "
          << fixed << setprecision(4) << box.xmax << " "
          << fixed << setprecision(4) << box.ymin << " "
          << fixed << setprecision(4) << box.ymax << " "
          << "-> ";
        cout << image_ids[u];
        last_image = u;
      } else {
        auto box = img_lbl_bbox[make_pair(last_image, u - I)];
        cout
          << " -> "
          << fixed << setprecision(4) << box.xmin << " "
          << fixed << setprecision(4) << box.xmax << " "
          << fixed << setprecision(4) << box.ymin << " "
          << fixed << setprecision(4) << box.ymax << " "
          << "| " << label_display_name[label_names[u - I]]
          << "\n";
        last_label = u - I;
      }
    }
    checkpoint_longest.clear();
    cout << endl;
    cout << endl;
  };

  for (int w = 0; w < n_walks; ++w) {
    auto elapsed = chrono::duration_cast<chrono::seconds>(
      chrono::steady_clock::now() - t_start).count();
    if (elapsed >= last_print_elapsed + 3) {
      cout << w << "/" << n_walks << " ("
        << fixed << setprecision(2) << (float)w / n_walks * 100
        << "%)" << endl << endl;
      print_stats(w);
      last_print_elapsed += 3;
    }

    int current = rand_int(rng_seed, I);
    unordered_set<int> visited;
    visited.insert(current);

    const auto nonhuman = [I, &label_names, &label_display_name] (int u) -> bool {
      if (u < I) return true;
      return label_display_name[label_names[u - I]].find("Human") == std::string::npos;
    };

    vector<int> walk;
    int last_image = current;
    for (int step = 0; step < MAX_WALK_LEN - 1; ++step) {
      walk.push_back(current);
      // Collect unvisited neighbors
      vector<int> candidates;
      for (int nb : graph[current]) {
        if (visited.find(nb) == visited.end() &&
            (nb < I ? true : (/* nonhuman(nb) && */ (img_labels[nb] ^ img_labels[last_image]).count() <= 2)))
          candidates.push_back(nb);
      }
      if (candidates.empty()) break;  // no unvisited neighbor, stop

      // Randomly pick among unvisited neighbors
      int next = candidates[rand_int(rng_seed, candidates.size())];
      visited.insert(next);
      current = next;
      if (current < I) last_image = current;
    }
    walk_lengths[(int)walk.size()] += 1;
    if (walk.size() > checkpoint_longest.size())
      checkpoint_longest = walk;
  }

  print_stats(n_walks);

  return 0;
}
