// gcc -O3 cwatch.c -o cwatch

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>
#include <sys/timeb.h>
#include <string.h>
#include <unistd.h>

#define NUM_COLORS 5

#define MATCH_SCORE  10
#define INSERT_SCORE  1
#define DELETE_SCORE  1
#define ERROR_SCORE   4

#define STATUS_GOOD 0
#define STATUS_EMPTY_STRING 1
#define STATUS_WINDOW_SIZE_TOO_SMALL 2

#define DEFAULT_MAX_STR 10000
#define DEFAULT_HISTORY_SIZE 10000
#define DEFAULT_HALF_WINDOW_SIZE 400
#define DEFAULT_DELAY 2.0
#define SMALL_WAIT 0.01
#define TEN_TO_THE_NINE 1000000L
#define MAX_TIME_STR 100

int max_str;
int half_window_size;
int window_size;
int history;

int ansi_color[NUM_COLORS] = {31, 33, 32, 36, 35};
int color_pos[NUM_COLORS+1] = {1, 10, 100, 1000, 10000};

#define VINDEX(y,w) ((y) * window_size + (w))
int * score;
int * prev_x;
int * prev_y;
int * match;

#define RINDEX(h,i) ((h) * max_str + (i))
unsigned char * ring_buffer;
double * frame_time;
int rb_current = 0;
int rb_size = 0;

int * model_color;

void run_command(char * command, char * buffer){
  FILE * fp;
  int buffer_size;

  fp = popen(command, "r");
  buffer_size = fread(buffer, 1, max_str - 1, fp);
  buffer[buffer_size] = 0;
  pclose(fp);  
}

int get_color(int index) {
  int i;

  if (index==0) return -1;
  if (index==1) return 0;
  
  for(i=1;i<NUM_COLORS;i++) {
    if ((index > color_pos[i-1]) && (index <= color_pos[i])) return i;
  }
}

// [y][w] --> (w+y-half_window_size, y)
// (x,y)  --> [y][x-y+half_window_size]
int edit_distance_color(unsigned char * model, unsigned char * old, int * color, int fill_color, int * difference_flag) {
  int i,x,y,w,xx,yy,ww,ss,mm,best_i,best_ds,best_mm,best_xx,best_yy,best_ww,len_model,len_old;
  int dx[3] = {1,0,1};
  int dy[3] = {1,1,0};
  int ds[3] = {MATCH_SCORE, DELETE_SCORE, INSERT_SCORE};
  int dm[3] = {1,0,0};

  *difference_flag = 0;

  len_model = strlen(model);
  len_old = strlen(old);
  if (len_model==0 || len_old==0) {
    // Empty string, bail.
    for(y=0;y<len_model;y++)
      color[y] = fill_color;
    *difference_flag = 1;
    return STATUS_EMPTY_STRING;      
  }

  // Forward pass
  score[VINDEX(0,0)] = 0;
  match[VINDEX(0,0)] = model[0] == old[0];
  for(y=0;y<len_model;y++)
    for(w=0;w<window_size;w++) {
      x=w+y-half_window_size;
      if (x<0 || x>=len_old) continue;
      if (x==0 && y==0) continue;
      best_ds = 0;
      best_i = 0;      
      for(i=0;i<3;i++) {
	xx = x - dx[i];
	yy = y - dy[i];
	ww = xx - yy + half_window_size;
	if (xx<0 || yy<0 || ww<0 || ww>=window_size) continue;
	ss = ds[i];
	mm = dm[i];
	if (i==0 && (model[y] != old[x])) {
	  ss=ERROR_SCORE;
	  mm=0;
	}
	if (score[VINDEX(yy,ww)]+ss>best_ds) {
	  best_i = i;
	  best_ds = score[VINDEX(yy,ww)] + ss;
	  best_mm = mm;
	  best_xx = xx;
	  best_yy = yy;
	  best_ww = ww;
	}
      }
      score[VINDEX(y,w)] = best_ds;
      prev_x[VINDEX(y,w)] = best_xx;
      prev_y[VINDEX(y,w)] = best_yy;
      match[VINDEX(y,w)] = best_mm;
    }
  
  // Backward pass
  for(xx=len_old-1,yy=len_model-1;xx>0 || yy>0;) {
    ww = xx - yy + half_window_size;
    if (ww<0 || ww >= window_size) {
      // window_size too small, bail.
      for(y=0;y<len_model;y++)
	color[y] = fill_color;
      *difference_flag = 1;
      return STATUS_WINDOW_SIZE_TOO_SMALL;
    }
    if (!match[VINDEX(yy,ww)]) {
      color[yy] = fill_color;
      *difference_flag = 1;
    }
    if (yy == 0 && ww <= half_window_size) break;
    y = prev_y[VINDEX(yy,ww)];
    x = prev_x[VINDEX(yy,ww)];
    yy = y;
    xx = x;
  }

  return STATUS_GOOD;
}

void cprint(unsigned char * model, int * color, double duration, int status, double time_since_most_recent_change, double time_since_least_recent_change) {
  int i,j;
  int current_color = -1;
  time_t tm;
  char time_str[MAX_TIME_STR];

  strcpy(time_str,ctime(&tm));
  time_str[strlen(time_str)-1] = 0;
  tm = time(NULL);
  printf("\033[34m");
  printf("-----------------------------------------------------\n");
  printf("%6.2f | %s | %6.2f | %6.2f\n", duration, time_str, time_since_most_recent_change, time_since_least_recent_change);
  if (status == STATUS_WINDOW_SIZE_TOO_SMALL) {
    printf("\033[31m");
    printf("window_size_TOO_SMALL - consider increasing it\n");
    printf("\033[34m");
  } else if (status == STATUS_EMPTY_STRING) {
    printf("\033[31m");
    printf("EMPTY STRING\n");
    printf("\033[34m");
  }
  if (strlen(model)>=max_str-1) {
    printf("\033[31m");
    printf("STRING TRUNCATED - consider increasing max_string_size\n");
    printf("\033[34m");
  }
  printf("-----------------------------------------------------\n");
  printf("\033[0m");
  
  for(i=0;i<strlen(model);i++) {
    if (color[i]!=current_color) {
      if (color[i]==-1) {
	printf("\033[0m");
      } else {
	printf("\033[%dm", ansi_color[color[i]]);	
      }
      current_color = color[i];
    }
    if ((color[i] != -1) && (model[i]==' ')) {
      printf("_");
    } else if ((color[i] != -1) && (model[i]=='\t')) {
      for(j=0;j<8;j++) printf("_");
    } else if ((color[i] != -1) && (model[i]=='\n')) {
      printf("/\n");
    } else {
      printf("%c",model[i]);
    }
  }
  if (model[strlen(model)-1] != '\n') printf("\n");
}
 
double get_seconds(){
   struct timeb tm;
   tm.timezone = 0;
   tm.dstflag = 0;
  ftime(&tm);
  return tm.time + tm.millitm * 0.001;
}

void argparse(int argc, char ** argv, double * delay, int * max_str, int * half_window_size, int * history) {
  int i;
  
  *delay = DEFAULT_DELAY;
  *max_str = DEFAULT_MAX_STR;
  *half_window_size = DEFAULT_HALF_WINDOW_SIZE;
  *history = DEFAULT_HISTORY_SIZE;
  
  if (argc <= 1) {
    printf("Usage:\n");
    printf(" cwatch [options] command\n");
    printf("\n");
    printf("Options:\n");
    printf("  -d                      delay (default = 2 sec)\n");
    printf("  -w                      window_size (default = 400)\n");
    printf("  -s                      max_string_size (default = 10000)\n");
    printf("  -h                      history to store (default = 10000)\n");
    printf("\n");
    
    exit(1);
  }
  
  for(i=1;i<argc-2;i++) {
    if (!strcmp(argv[i],"-d")) {
      *delay = atof(argv[i+1]);
    }
  }
  
  for(i=1;i<argc-2;i++) {
    if (!strcmp(argv[i],"-s")) {
      *max_str = atoi(argv[i+1]);
    }
  }  
  
  for(i=1;i<argc-2;i++) {
    if (!strcmp(argv[i],"-w")) {
      *half_window_size = atoi(argv[i+1]);
    }
  }
  
  for(i=1;i<argc-2;i++) {
    if (!strcmp(argv[i],"-h")) {
      *history = atoi(argv[i+1]);
    }
  }
}

int main(int argc, char ** argv) {
  int i, j, h, status;
  double tic, toc;
  double delay;
  int difference_flag;
  double time_since_most_recent_change;
  double time_since_least_recent_change;
  double interval;

  argparse(argc, argv, &delay, &max_str, &half_window_size, &history);
  window_size = 2*half_window_size;

  if ((score = malloc(sizeof(int) * max_str * window_size))==NULL) {fprintf(stderr,"Out of memory\n");}
  if ((prev_x = malloc(sizeof(int) * max_str * window_size))==NULL) {fprintf(stderr,"Out of memory\n");}
  if ((prev_y = malloc(sizeof(int) * max_str * window_size))==NULL) {fprintf(stderr,"Out of memory\n");}
  if ((match = malloc(sizeof(int) * max_str * window_size))==NULL) {fprintf(stderr,"Out of memory\n");}

  if ((model_color = malloc(sizeof(int) * max_str))==NULL) {fprintf(stderr,"Out of memory\n");}

  if ((ring_buffer = malloc(sizeof(unsigned char) * history * max_str))==NULL) {fprintf(stderr,"Out of memory\n");}
  if ((frame_time = malloc(sizeof(double) * history))==NULL) {fprintf(stderr,"Out of memory\n");}

  
  tic = get_seconds();
  while(1) {
    toc = tic;
    tic = get_seconds();
    run_command(argv[argc-1], &ring_buffer[RINDEX(rb_current,0)]);
    frame_time[rb_current] = get_seconds();
    if (++rb_size >= history) rb_size = history;
    for(i=0;i<max_str;i++) model_color[i] = -1;
    time_since_most_recent_change = 0;
    time_since_least_recent_change = 0;
    for(h=rb_size-1;h>0;h--) {
      j = rb_current - h;
      if (j < 0) j += history;
      status = edit_distance_color(&ring_buffer[RINDEX(rb_current,0)], &ring_buffer[RINDEX(j,0)], model_color, get_color(h), &difference_flag);
      if (difference_flag) {
	interval = frame_time[rb_current] - frame_time[j];
	time_since_most_recent_change = interval;
	if (!time_since_least_recent_change) time_since_least_recent_change = interval;
      }
    }
    while (tic - toc < delay) {
      usleep(SMALL_WAIT * TEN_TO_THE_NINE);
      tic = get_seconds();
    }
    cprint(&ring_buffer[RINDEX(rb_current,0)], model_color, tic - toc, status, time_since_most_recent_change, time_since_least_recent_change);
    if (++rb_current >= history) rb_current = 0;
  }
}
