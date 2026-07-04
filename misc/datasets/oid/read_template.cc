#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>

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

// trim both ends
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

  bbox_file_name =
#if 1
    "/tmp/bbox-part.csv";
#else
    "oidv6-train-annotations-bbox.csv";
#endif
  labeldesc_file_name = "oidv7-class-descriptions-boxable.csv";

  // ---------- 1. Read annotations ----------
  ifstream file(bbox_file_name);
  if (!file.is_open()) {
    cerr << "Cannot open " << bbox_file_name << endl;
    return 1;
  }

  cout << "Reading bounding box annotations from " << bbox_file_name << endl;

  struct bbox {
    float xmin, xmax, ymin, ymax;
  };

  unordered_map<string, int> image_index;
  unordered_map<string, int> label_index;
  vector<string> image_ids;
  vector<string> label_names;

  string line;
  getline(file, line); // skip header
  while (getline(file, line)) {
    if (line.empty()) continue;
    auto cols = split(line, ',');
    if (cols.size() < 8) continue;
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
    } else {
      lbl_idx = it_lbl->second;
    }
  }
  file.close();

  int I = image_ids.size();
  int L = label_names.size();
  cout << "Images: " << I << ", Labels: " << L << endl;

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
      line = trim(line);
      if (line.empty()) continue;
      auto cols = split(line, ',');
      if (cols.size() >= 2)
        label_display_name[cols[0]] = cols[1];
    }
  }

  // ---------- 3. Accept requests ----------
  cout << "Ready for requests" << endl;
  string query_line;
  while (getline(cin, query_line)) {
    query_line = trim(query_line);
    if (query_line.empty()) continue;
    continue;
  }

  return 0;
}
