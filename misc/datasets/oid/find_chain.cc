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
#include <queue>          // added for BFS

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

#include <unistd.h>
#include <termios.h>

int set_canonical_mode() {
  struct termios tty_settings;

  // 1. Get current terminal attributes for standard input
  if (tcgetattr(STDIN_FILENO, &tty_settings) != 0) {
    perror("tcgetattr failed");
    return -1;
  }

  // 2. Set the ICANON bit to enable canonical mode
  tty_settings.c_lflag |= ICANON;

  // Optional: Turn standard local echo back on if it was disabled
  tty_settings.c_lflag |= ECHO;

  // Set erase character to Backspace
  tty_settings.c_cc[VERASE] = 0x08;

  // 3. Apply the modified attributes immediately
  if (tcsetattr(STDIN_FILENO, TCSANOW, &tty_settings) != 0) {
    perror("tcsetattr failed");
    return -1;
  }

  return 0;
}

// --------------------------------------
int main(int argc, char *argv[]) {
  set_canonical_mode();
  string bbox_file_name;
  string labeldesc_file_name;

  bbox_file_name =
#if 1
    "/tmp/bbox-part.csv";
#else
    "oidv6-train-annotations-bbox.csv";
#endif
  labeldesc_file_name = "oidv7-class-descriptions-boxable.csv";

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

    // Add edge (both sides) – only small non‑human objects
    if (
      (box.xmax - box.xmin) * (box.ymax - box.ymin) < 0.1 &&
      true // label_display_name[label_names[lbl_idx]].find("Human") == std::string::npos
    ) {
      img_adj[img_idx].insert(lbl_idx);
      lbl_adj[lbl_idx].insert(img_idx);
    }

    // Record all labels of the image (regardless of the filter)
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
  unordered_map<string, int> display_to_labels;
  for (int i = 0; i < L; ++i) {
    auto it = label_display_name.find(label_names[i]);
    if (it != label_display_name.end()) {
      display_to_labels[it->second] = i;
    }
  }

  // ---------- 5. Query loop ----------
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
    if (it1 == display_to_labels.end()) {
      cout << "Unknown label: " << name1 << endl;
      continue;
    } if (it2 == display_to_labels.end()) {
      cout << "Unknown label: " << name2 << endl;
      continue;
    }

    // Try every combination of start / target label indices
    bool found = false;
    vector<int> path_vertices;   // will hold the alternating chain

    auto t_start = chrono::steady_clock::now();

    int start_label = it1->second;
    int target_label = it2->second;

    // Simple case: same label
    if (start_label == target_label) {
      found = true;
      path_vertices = {I + start_label};   // label vertex
      break;
    }

    // ---------- BFS with image‑difference constraint ----------
    // State: (vertex, last_image)   -1 means no image yet
    queue<pair<int, int>> q;
    unordered_map<pair<int, int>, pair<int, int>, pair_hash> parent;
    unordered_set<pair<int, int>, pair_hash> visited;

    pair<int, int> start_state = {I + start_label, -1};
    q.push(start_state);
    visited.insert(start_state);
    parent[start_state] = {-1, -1};   // sentinel

    while (!q.empty()) {
      auto [v, last_img] = q.front();
      q.pop();

      if (v == I + target_label) {   // reached target label vertex
        found = true;
        // reconstruct path
        pair<int, int> cur = {v, last_img};
        while (cur != make_pair(-1, -1)) {
          path_vertices.push_back(cur.first);
          cur = parent[cur];
        }
        reverse(path_vertices.begin(), path_vertices.end());
        break;
      }

      if (v >= I) {   // label vertex -> image neighbours
        int L_idx = v - I;
        for (int I_next : graph[v]) {
          if (last_img != -1) {
            // Constraint: I_last and I_next share at most 2 labels
            if ((img_labels[last_img] & img_labels[I_next]).count() > 2)
              continue;
          }
          pair<int, int> nxt = {I_next, I_next};
          if (visited.find(nxt) == visited.end()) {
            visited.insert(nxt);
            parent[nxt] = {v, last_img};
            q.push(nxt);
          }
        }
      } else {        // image vertex -> label neighbours
        for (int L_next : graph[v]) {
          pair<int, int> nxt = {L_next, v};
          if (visited.find(nxt) == visited.end()) {
            visited.insert(nxt);
            parent[nxt] = {v, last_img};
            q.push(nxt);
          }
        }
      }

      if (chrono::duration_cast<chrono::seconds>(
      chrono::steady_clock::now() - t_start).count() >= 10) break;
    }

    // ---------- Print the chain ----------
    if (found) {
      cout << "Chain found (" << path_vertices.size() << " vertices):" << endl;
      if (0) for (size_t i = 0; i < path_vertices.size(); ++i) {
        int v = path_vertices[i];
        if (v >= I) {   // label
          int lbl = v - I;
          string dname = label_display_name.count(label_names[lbl]) ?
                         label_display_name[label_names[lbl]] : "?";
          cout << "L" << lbl << "(" << dname << ")";
        } else {        // image
          cout << "I" << v << "(" << image_ids[v] << ")";
        }
        if (i + 1 < path_vertices.size()) cout << " -> ";
      }

      int n = path_vertices.size() / 2;
      const auto image = [&] (int i) -> int {
        if (i < 0) i = 0;
        if (i >= n) i = n - 1;
        return path_vertices[i * 2 + 1];
      };
      const auto label = [&] (int i) -> int {
        if (i < 0) return path_vertices[0] - I;
        if (i >= n - 1) return path_vertices[n * 2] - I;
        return path_vertices[i * 2 + 2] - I;
      };
      for (int i = -1; i <= n; ++i) {
        int img = image(i);
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
    cout << endl;
  } else {
    cerr << "No chain found between '" << name1 << "' and '" << name2 << "'." << endl;
  }
}

return 0;
}
