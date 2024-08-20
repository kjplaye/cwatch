#include <bits/time.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define NUM_COLORS 5
#define NUM_ANSI_COLORS 7
#define MAX_STR 100

#define MATCH_SCORE 10
#define INSERT_SCORE 1
#define DELETE_SCORE 1
#define ERROR_SCORE 4

#define STATUS_GOOD 0
#define STATUS_EMPTY_STRING 1
#define STATUS_WINDOW_SIZE_TOO_SMALL 2

#define DEFAULT_MAX_STR 10000
#define DEFAULT_HISTORY_SIZE 500
#define DEFAULT_HALF_WINDOW_SIZE 400
#define DEFAULT_DELAY 2.0
#define SMALL_WAIT 0.01
#define TEN_TO_THE_NINE 1000000L

// Global options.
int max_str;
int half_window_size;
int history;
int clear_terminal = 1;

// The ansi color codes and string representations.
int ansi_color[NUM_COLORS] = {31, 33, 32, 36, 35};
char ansi_str[NUM_ANSI_COLORS][MAX_STR] = {
    "black", "red", "green", "yellow", "blue", "magenta", "cyan"};

// The number of steps for each color.
int color_pos[NUM_COLORS + 1] = {1, 5, 25, 125, 625};

// Indexing and definition of Viterbu arrays
//     score, prev_x, prev_y, and match.
#define VINDEX(y, w) ((y)*window_size + (w))
int *score;
int *prev_x;
int *prev_y;
int *match;

// Indexing and definition of ring_buffer.
#define RINDEX(h, i) ((h)*max_str + (i))
char *ring_buffer;

// Run the command and grab the input in buffer.
void run_command(char *command, char *buffer) {
  // Open the process running the command and grabbing the output.
  FILE *fp = popen(command, "r");
  int buffer_size = fread(buffer, 1, max_str - 1, fp);
  buffer[buffer_size] = 0;

  // Close the process.
  pclose(fp);
}

// Return a color for 1 <= index < rb_size
int get_color(int index) {
  if (index == 0) return -1;
  if (index == 1) return 0;

  for (int i = 1; i < NUM_COLORS; i++) {
    if ((index > color_pos[i - 1]) && (index <= color_pos[i]))
      return i;
  }

  return 0;
}

// [y][w] --> (w+y-half_window_size, y)
// (x,y)  --> [y][x-y+half_window_size]
int edit_distance_color(char *model, char *old, int *color,
			int fill_color, int *difference_flag,
			int half_window_size) {
  int dx[3] = {1, 0, 1};
  int dy[3] = {1, 1, 0};
  int ds[3] = {MATCH_SCORE, DELETE_SCORE, INSERT_SCORE};
  int dm[3] = {1, 0, 0};

  // A convenient alias.
  int window_size = 2 * half_window_size;

  // Set this to 1 if a change is made.
  *difference_flag = 0;

  // Get length of model and old string to compare,
  // bail if either is zero.
  int len_model = strnlen(model, max_str);
  int len_old = strnlen(old, max_str);
  if (len_model == 0 || len_old == 0) {
    // Empty string, set output color to .
    for (int y = 0; y < len_model; y++) color[y] = fill_color;
    *difference_flag = 1;
    return STATUS_EMPTY_STRING;
  }

  // Forward pass
  score[VINDEX(0, 0)] = 0;
  match[VINDEX(0, 0)] = model[0] == old[0];
  for (int y = 0; y < len_model; y++)
    for (int w = 0; w < window_size; w++) {
      int x = w + y - half_window_size;
      if (x < 0 || x >= len_old)
        continue;
      if (x == 0 && y == 0)
        continue;
      
      int best_ds = 0;
      int best_i = 0;
      int best_mm = 0;
      int best_xx = 0;
      int best_yy = 0;
      int best_ww = 0;       
      for (int i = 0; i < 3; i++) {
        int xx = x - dx[i];
        int yy = y - dy[i];
        int ww = xx - yy + half_window_size;
        if (xx < 0 || yy < 0 || ww < 0 || ww >= window_size)
          continue;
        int ss = ds[i];
        int mm = dm[i];
        if (i == 0 && (model[y] != old[x])) {
          ss = ERROR_SCORE;
          mm = 0;
        }
        if (score[VINDEX(yy, ww)] + ss > best_ds) {
          best_i = i;
          best_ds = score[VINDEX(yy, ww)] + ss;
          best_mm = mm;
          best_xx = xx;
          best_yy = yy;
          best_ww = ww;
        }
      }
      score[VINDEX(y, w)] = best_ds;
      prev_x[VINDEX(y, w)] = best_xx;
      prev_y[VINDEX(y, w)] = best_yy;
      match[VINDEX(y, w)] = best_mm;
    }

  // Backward pass
  for (int xx = len_old - 1, yy = len_model - 1; xx > 0 || yy > 0;) {
    int ww = xx - yy + half_window_size;
    if (ww < 0 || ww >= window_size) {
      // window_size too small, bail.
      for (int y = 0; y < len_model; y++)
        color[y] = fill_color;
      *difference_flag = 1;
      return STATUS_WINDOW_SIZE_TOO_SMALL;
    }
    if (!match[VINDEX(yy, ww)]) {
      color[yy] = fill_color;
      *difference_flag = 1;
    }
    if (yy == 0 && ww <= half_window_size)
      break;

    int y = prev_y[VINDEX(yy, ww)];
    int x = prev_x[VINDEX(yy, ww)];
    yy = y;
    xx = x;
  }

  return STATUS_GOOD;
}

// Print the colorized output. 
void cprint(char *model, int *color, double duration, int status,
            double time_since_most_recent_change, int count) {
  // Clear the terminal with ANSI "alternate screen buffer".
  if (clear_terminal) printf("\033[?1049h\033[H");

  // Read the time into a string.
  char time_str[MAX_STR];
  time_t tm = time(NULL);
  strcpy(time_str, ctime(&tm));
  time_str[strnlen(time_str, max_str) - 1] = 0;

  // Print banner.
  printf("\033[34m");
  printf("--------------------------------------------------\n");
  printf("%6.2f | %s | %6.2f (%d)\n", duration, time_str,
         time_since_most_recent_change, count);

  // Warn if the window size is too small
  if (status & STATUS_WINDOW_SIZE_TOO_SMALL) {
    printf("\033[31m");
    printf("window-size too small - consider increasing it\n");
    printf("\033[34m");
    printf("MEOW %d\n", status);
  }

  // Warn if an empty string is input.
  if (status & STATUS_EMPTY_STRING) {
    printf("\033[31m");
    printf("An output is an empty string\n");
    printf("\033[34m");
  }

  // Warn if the output is too large.
  if (strnlen(model, max_str) >= max_str - 1) {
    printf("\033[31m");
    printf("STRING TRUNCATED - consider increasing max_string_size\n");
    printf("\033[34m");
  }

  // End banner.
  printf("--------------------------------------------------\n");
  printf("\033[0m");

  // Output the colorized output.
  int current_color = -1;
  for (int i = 0; i < strnlen(model, max_str); i++) {
    if (color[i] != current_color) {
      // Print the appropriate ANSI color code.
      if (color[i] == -1) {
	// Reset to original color.
        printf("\033[0m");
      } else {
	// The color code.
        printf("\033[%dm", ansi_color[color[i]]);
      }
      current_color = color[i];
    }

    // Print the single character from the output (model).
    printf("%c", model[i]);
  }

  // If missing in the output, add a newline.
  if (model[strlen(model) - 1] != '\n')
    printf("\n");
}

// Get the number of seconds since the Epoch.
double get_seconds() {
  // Get the timespec structure;
  struct timespec ct;
  clock_gettime(CLOCK_REALTIME, &ct);

  // Return the number of seconds.
  double seconds = ct.tv_sec + ct.tv_nsec * 1e-9;
  return seconds;
}

void argparse(int argc, char **argv, double *delay, int *max_str,
              int *half_window_size, int *history) {
  // Set options to defaults.
  *delay = DEFAULT_DELAY;
  *max_str = DEFAULT_MAX_STR;
  *half_window_size = DEFAULT_HALF_WINDOW_SIZE;
  *history = DEFAULT_HISTORY_SIZE;

  // Check to see if there is at least one argument, else print usage.
  if (argc <= 1) {
    printf("Usage:\n");
    printf(" cwatch [options] command\n");
    printf("\n");
    printf("Options:\n");
    printf("  -c                      don't clear terminal after commands\n");
    printf("  -d                      delay (default = 2 sec)\n");
    printf("  -h                      history to store (default = 10000)\n");
    printf("  -p                      pick color diff color, color blind "
           "option\n");
    printf("                              (");
    for (int j = 0; j < NUM_ANSI_COLORS; j++) {
      if (j)
        printf(", ");
      printf("%s", ansi_str[j]);
    }
    printf(")\n");
    printf("  -s                      max_string_size (default = 10000)\n");
    printf("  -w                      window_size (default = 400)\n");
    printf("\n");

    exit(1);
  }

  // Read option: clear terminal.  
  for (int i = 1; i < argc - 1; i++) {
    if (!strcmp(argv[i], "-c")) {
      clear_terminal = 0;
    }
  }

  // Read option: delay.
  for (int i = 1; i < argc - 2; i++) {
    if (!strcmp(argv[i], "-d")) {
      *delay = atof(argv[i + 1]);
    }
  }

  // Read option: history length.  
  for (int i = 1; i < argc - 2; i++) {
    if (!strcmp(argv[i], "-h")) {
      *history = atoi(argv[i + 1]);
    }
  }

  // Read option: max string size.  
  for (int i = 1; i < argc - 2; i++) {
    if (!strcmp(argv[i], "-s")) {
      *max_str = atoi(argv[i + 1]);
    }
  }

  // Read option: (half) window size.
  for (int i = 1; i < argc - 2; i++) {
    if (!strcmp(argv[i], "-w")) {
      *half_window_size = atoi(argv[i + 1]);
    }
  }

  // Handle option: picked color.
  for (int i = 1; i < argc - 2; i++) {
    if (!strcmp(argv[i], "-p")) {
      char *picked_color_str = argv[i + 1];
      int j;
      for (j = 0; j < NUM_ANSI_COLORS; j++) {
        if (strncmp(picked_color_str, ansi_str[j], MAX_STR) == 0)
          break;
      }
      if (j >= NUM_ANSI_COLORS) {
        fprintf(stderr, "Picked color '%s' does not match known ansi colors\n",
                picked_color_str);
        exit(1);
      }
      for (int k = 0; k < NUM_COLORS; k++)
        ansi_color[k] = 30 + j;
    }
  }
}

int main(int argc, char **argv) {
  // Parse arguments to get delay, max_str, window_size and
  // history length.
  double delay;
  argparse(argc, argv, &delay, &max_str, &half_window_size, &history);

  // Set up viterbi arrays.
  int window_size = 2 * half_window_size;
  if ((score = malloc(sizeof(int) * max_str * window_size)) == NULL) {
    fprintf(stderr, "Out of memory\n");
  }
  if ((prev_x = malloc(sizeof(int) * max_str * window_size)) == NULL) {
    fprintf(stderr, "Out of memory\n");
  }
  if ((prev_y = malloc(sizeof(int) * max_str * window_size)) == NULL) {
    fprintf(stderr, "Out of memory\n");
  }
  if ((match = malloc(sizeof(int) * max_str * window_size)) == NULL) {
    fprintf(stderr, "Out of memory\n");
  }

  // Set up model array (Indexes column of Viterbi array).
  int *model_color;
  if ((model_color = malloc(sizeof(int) * max_str)) == NULL) {
    fprintf(stderr, "Out of memory\n");
  }

  // Set up ring buffer of length history of output strings.
  // rb_current points to the next time index to read (wraps around).
  // rb_size is the number of strings stored (max is history).
  int rb_current = 0;
  int rb_size = 0;
  if ((ring_buffer = malloc(sizeof(char) * history * max_str))
      == NULL) {
    fprintf(stderr, "Out of memory\n");
  }

  // The seconds since epoch (indexed the same as ring_buffer.)
  double *frame_time;
  if ((frame_time = malloc(sizeof(double) * history)) == NULL) {
    fprintf(stderr, "Out of memory\n");
  }

  // Main watch loop.
  double tic = get_seconds();
  double toc = tic;
  int count = 0;
  while (1) {
    double time_since_most_recent_change; // Used in banner.

    // Start timer, run command, and get result in ring buffer.
    tic = toc;
    toc = get_seconds();
    run_command(argv[argc - 1], &ring_buffer[RINDEX(rb_current, 0)]);
    frame_time[rb_current] = get_seconds();

    // Increase the length of the ring_buffer.
    // rb_current now points to most recent output.
    if (++rb_size >= history) rb_size = history;

    // Remove color from the model.
    for (int i = 0; i < max_str; i++) model_color[i] = -1;

    // Loop over all history computing the edit distance.
    time_since_most_recent_change = 0.0;
    int status = 0;
    for (int h = rb_size - 1; h > 0; h--) {
      // Set j to be the index of history h back.
      int j = rb_current - h;
      if (j < 0) j += history;

      // Compute the edit distance
      int difference_flag;
      status |= edit_distance_color(&ring_buffer[RINDEX(rb_current, 0)],
				    &ring_buffer[RINDEX(j, 0)],
				    model_color,
				    get_color(h), &difference_flag,
				    half_window_size);

      // If there is a difference, update time since change.
      if (difference_flag)
        time_since_most_recent_change =
	  frame_time[rb_current] - frame_time[j];
    }

    // Wait for delay.
    while (toc - tic < delay) {
      usleep((delay - (toc - tic) + SMALL_WAIT) * TEN_TO_THE_NINE);
      toc = get_seconds();
    }

    // Print out the colorized most recent output.
    cprint(&ring_buffer[RINDEX(rb_current, 0)], model_color,
	   toc - tic, status, time_since_most_recent_change, count++);

    // Advance the ring buffer.
    // rb_current will point to next new position.
    if (++rb_current >= history) rb_current = 0;
  }

  free(score);
  free(prev_x);
  free(prev_y);
  free(match);
  free(model_color);
}
