// cc filter_by_imageid.c -o /tmp/f -O2 && echo -e "2a5b1594c3470251\n0e7debb62c4524c0" | /tmp/f

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define MAX_LINE_LEN 4096
#define IMAGE_ID_LEN 16

// Function to extract ImageID from a line
int extract_image_id(const char *line, char *image_id) {
  // Skip any leading whitespace
  while (isspace(*line)) line++;

  // Copy up to 16 characters or until comma
  int i = 0;
  while (line[i] && line[i] != ',' && i < IMAGE_ID_LEN) {
    image_id[i] = line[i];
    i++;
  }
  image_id[i] = '\0';

  // Check if we got exactly 16 chars and stopped at comma or end
  if (i == 0 || (line[i] != ',' && line[i] != '\0')) {
    return 0; // Invalid format
  }
  return 1;
}

// Get the position of the start of the current line
long get_line_start(FILE *fp, long pos) {
  char c;
  long start = pos;

  // If pos is 0, we're at the beginning
  if (pos == 0) return 0;

  // Go back to find the newline before this position
  while (start > 0) {
    fseek(fp, start - 1, SEEK_SET);
    c = fgetc(fp);
    if (c == '\n') {
      return start;
    }
    start--;
  }
  return 0;
}

// Get the position of the start of the next line
long get_next_line_start(FILE *fp, long pos) {
  char c;
  long current = pos;

  fseek(fp, current, SEEK_SET);
  while ((c = fgetc(fp)) != EOF) {
    if (c == '\n') {
      return ftell(fp);
    }
    current++;
  }
  return ftell(fp);
}

// Get the first line of the file (header)
int get_header(FILE *fp, char *header, size_t max_len) {
  rewind(fp);
  if (fgets(header, max_len, fp) == NULL) {
    return 0;
  }
  return 1;
}

// Binary search for the given ImageID
long binary_search(FILE *fp, const char *target_id, long start_pos, long end_pos) {
  char line[MAX_LINE_LEN];
  char current_id[IMAGE_ID_LEN + 1];
  long mid_pos;
  long line_start;
  int iterations = 0;

  while (start_pos <= end_pos && iterations < 100) {
    iterations++;
    mid_pos = start_pos + (end_pos - start_pos) / 2;

    // Find the start of the line at or before mid_pos
    line_start = get_line_start(fp, mid_pos);

    // If line_start is 0 or we're at the beginning, we might be at the header
    // Skip the header if we're at position 0
    if (line_start == 0) {
      // Check if we're at the header
      fseek(fp, 0, SEEK_SET);
      if (fgets(line, sizeof(line), fp) == NULL) {
        return -1;
      }
      // If this is the header, move past it
      if (strstr(line, "ImageID") != NULL) {
        line_start = ftell(fp);
      }
    }

    // Seek to the line start
    fseek(fp, line_start, SEEK_SET);

    // Read the line
    if (fgets(line, sizeof(line), fp) == NULL) {
      // End of file reached
      end_pos = mid_pos - 1;
      continue;
    }

    // Extract ImageID
    if (!extract_image_id(line, current_id)) {
      // Invalid line, move to next line
      long next_start = get_next_line_start(fp, ftell(fp));
      if (next_start > end_pos) {
        end_pos = mid_pos - 1;
      } else {
        start_pos = next_start;
      }
      continue;
    }

    // Compare IDs
    int cmp = strcmp(current_id, target_id);

    if (cmp == 0) {
      // Found it! Now find the first occurrence
      long found_pos = line_start;

      // Go backward to find the first occurrence
      while (found_pos > 0) {
        long prev_line_start = get_line_start(fp, found_pos - 1);
        if (prev_line_start == found_pos) break;

        fseek(fp, prev_line_start, SEEK_SET);
        if (fgets(line, sizeof(line), fp) == NULL) break;

        if (!extract_image_id(line, current_id)) break;

        if (strcmp(current_id, target_id) == 0) {
          found_pos = prev_line_start;
        } else {
          break;
        }
      }

      return found_pos;
    } else if (cmp < 0) {
      // Target is greater, search right half
      long next_start = get_next_line_start(fp, ftell(fp));
      start_pos = next_start;
    } else {
      // Target is smaller, search left half
      end_pos = line_start - 1;
    }
  }

  return -1; // Not found
}

// Function to find end of file position
long get_file_size(FILE *fp) {
  fseek(fp, 0, SEEK_END);
  long size = ftell(fp);
  fseek(fp, 0, SEEK_SET);
  return size;
}

// Validate hex string
int is_valid_hex(const char *str, size_t len) {
  for (size_t i = 0; i < len; i++) {
    if (!isxdigit(str[i])) {
      return 0;
    }
  }
  return 1;
}

int main(int argc, char *argv[]) {
  const char *filename = "oidv6-train-annotations-bbox.csv";
  char target_id[IMAGE_ID_LEN + 3];
  char header[MAX_LINE_LEN];
  char line[MAX_LINE_LEN];
  FILE *fp;
  long file_size;
  long start_pos;

  // Open file
  fp = fopen(filename, "r");
  if (fp == NULL) {
    fprintf(stderr, "Error: Could not open file '%s'\n", filename);
    return 1;
  }

  // Get header
  if (!get_header(fp, header, sizeof(header))) {
    fprintf(stderr, "Error: Empty file\n");
    fclose(fp);
    return 1;
  }

  // Get file size for binary search range
  file_size = get_file_size(fp);

  // Skip header for binary search range
  long data_start = ftell(fp);

  while (!feof(stdin)) {
    // Read ImageID from stdin
    if (fgets(target_id, sizeof(target_id), stdin) == NULL) {
      if (feof(stdin)) break;
      fprintf(stderr, "Error reading input\n");
      return 1;
    }

    // Remove newline if present
    size_t len = strlen(target_id);
    if (len > 0 && target_id[len - 1] == '\n') {
      target_id[len - 1] = '\0';
      len--;
    }
    if (len == 0) continue;

    // Validate ImageID format (16 hex chars)
    if (len != IMAGE_ID_LEN) {
      fprintf(stderr, "Error: ImageID must be exactly 16 characters (got %zu)\n", len);
      return 1;
    }

    if (!is_valid_hex(target_id, IMAGE_ID_LEN)) {
      fprintf(stderr, "Error: ImageID must be hexadecimal characters only\n");
      return 1;
    }

    // Binary search for the target ID
    // fprintf(stderr, "Processing %s\n", target_id);
    start_pos = binary_search(fp, target_id, data_start, file_size);

    if (start_pos == -1) {
      printf("ImageID '%s' not found\n", target_id);
      fclose(fp);
      return 0;
    }

    // Print header
    // printf("%s", header);

    // Seek to the found position and print all matching lines
    fseek(fp, start_pos, SEEK_SET);
    while (fgets(line, sizeof(line), fp) != NULL) {
      char current_id[IMAGE_ID_LEN + 1];

      if (!extract_image_id(line, current_id)) {
        break;
      }

      if (strcmp(current_id, target_id) == 0) {
        printf("%s", line);
      } else {
        break; // Since file is sorted by ImageID, we can stop
      }
    }
  }

  fclose(fp);
  return 0;
}
