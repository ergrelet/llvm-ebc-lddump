#ifdef _WIN32

#include <Windows.h>
#include <stdio.h>
#include <string.h>

#define MAX_CMD_LINE 128 * 1024

char *get_command_line(size_t *output_size);
char *get_output_file_path(void *cmd_line, size_t cmd_line_size,
                           char separator);
void dump_command_line_to_file(char *output_path, char *cmd_line,
                               size_t cmd_line_size);

void my_init() {
  size_t cmd_line_size = 0;
  char *cmd_line = get_command_line(&cmd_line_size);
  if (cmd_line == NULL) {
    return;
  }

  // Extract output file path if present
  char *ld_output_file_path =
      get_output_file_path(cmd_line, cmd_line_size, '\0');
  if (ld_output_file_path == NULL) {
    // Set to 'a.exe' by default
    ld_output_file_path = "a.exe";
  }

  // Dump linker command-line into a file
  dump_command_line_to_file(ld_output_file_path, cmd_line, cmd_line_size);

  free(cmd_line);
}

char *get_command_line(size_t *output_size) {
  char *cmd_line = calloc(MAX_CMD_LINE, 1);
  if (cmd_line == NULL) {
    return NULL;
  }

  // Get the command line arguments as wchar_t strings
  int arg_count = 0;
  LPWSTR *arg_list = CommandLineToArgvW(GetCommandLineW(), &arg_count);
  if (arg_list == NULL) {
    free(cmd_line);
    return NULL;
  }

  char *current_arg_pos = cmd_line;
  for (size_t i = 0; i < arg_count; i++) {
    const int arg_size = WideCharToMultiByte(
        CP_UTF8, 0, arg_list[i], -1, current_arg_pos, MAX_CMD_LINE, NULL, NULL);
    current_arg_pos += arg_size;
  }
  LocalFree(arg_list);

  if (output_size != NULL) {
    *output_size = current_arg_pos - cmd_line;
  }

  return cmd_line;
}

// Retrieve output file path from the linker's command line.
// Returns NULL if it hasn't been found.
char *get_output_file_path(void *cmd_line, size_t cmd_line_size,
                           char separator) {
  void *next_nul = memchr(cmd_line, separator, cmd_line_size);
  while (next_nul != NULL) {
    char *option = (char *)next_nul + 1;
    // If current option starts with '-out:', this is the right one
    const size_t option_size = sizeof("-out:") - 1;
    if (strncmp(option, "-out:", option_size) == 0) {
      return option + option_size;
    }

    // Look for the next NUL char
    const size_t nul_offset = (char *)next_nul - (char *)cmd_line;
    next_nul = memchr(option, separator, cmd_line_size - nul_offset);
  }

  return NULL;
}

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved) {
  switch (fdwReason) {
    case DLL_PROCESS_ATTACH:
      my_init();
      break;

    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
    case DLL_PROCESS_DETACH:
      break;
  }

  return TRUE;
}

#else

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define MAX_CMD_LINE 128 * 1024
#define MAX_PATH 4096

char *get_command_line(size_t *output_size);
char *get_output_file_path(void *cmd_line, size_t cmd_line_size,
                           char separator);
void dump_command_line_to_file(char *output_path, char *cmd_line,
                               size_t cmd_line_size);

void __attribute__((constructor)) my_init() {
  size_t cmd_line_size = 0;
  char *cmd_line = get_command_line(&cmd_line_size);
  if (cmd_line == NULL) {
    return;
  }

  // Extract output file path if present
  char *ld_output_file_path =
      get_output_file_path(cmd_line, cmd_line_size, '\0');
  if (ld_output_file_path == NULL) {
    // Set to 'a.out' by default
    ld_output_file_path = "a.out";
  }

  // Dump linker command-line into a file
  dump_command_line_to_file(ld_output_file_path, cmd_line, cmd_line_size);

  free(cmd_line);
}

char *get_command_line(size_t *output_size) {
  char *cmd_line = calloc(MAX_CMD_LINE, 1);
  if (cmd_line == NULL) {
    return NULL;
  }

  // Retrieve the command line (options are separated by NUL chars)
  FILE *fp_in = fopen("/proc/self/cmdline", "rb");
  if (fp_in == NULL) {
    free(cmd_line);
    return NULL;
  }
  size_t read_bytes = fread(cmd_line, 1, MAX_CMD_LINE, fp_in);
  fclose(fp_in);

  // Ensure we could read the command line
  if (read_bytes == 0) {
    free(cmd_line);
    return NULL;
  }

  if (output_size != NULL) {
    *output_size = read_bytes;
  }

  return cmd_line;
}

// Retrieve output file path from the linker's command line.
// Returns NULL if it hasn't been found.
char *get_output_file_path(void *cmd_line, size_t cmd_line_size,
                           char separator) {
  int next_is_path = 0;
  void *next_nul = memchr(cmd_line, separator, cmd_line_size);
  while (next_nul != NULL) {
    char *option = (char *)next_nul + 1;
    if (next_is_path) {
      // `next_is_path` is set, we should return the current option
      return option;
    }

    // If current option is '-o', next option will be the output file path
    if (strcmp(option, "-o") == 0) {
      next_is_path = 1;
    }

    // Look for the next NUL char
    const size_t nul_offset = (char *)next_nul - (char *)cmd_line;
    next_nul = memchr(option, separator, cmd_line_size - nul_offset);
  }

  return NULL;
}

// GCC linker plugin interface
//
enum ld_plugin_status { LDPS_OK = 0, LDPS_NO_SYMS, LDPS_BAD_HANDLE, LDPS_ERR };
struct ld_plugin_tv;

enum ld_plugin_status onload(struct ld_plugin_tv *tv) { return LDPS_OK; }

#endif

void dump_command_line_to_file(char *output_path, char *cmd_line,
                               size_t cmd_line_size) {
  char output_file_path[MAX_PATH] = {'\0'};
  snprintf(output_file_path, sizeof(output_file_path), "%s.llvmldcmd",
           output_path);

  // Dump linker command-line into a file
  FILE *fp_out = fopen(output_file_path, "wb");
  if (fp_out == NULL) {
    return;
  }
  fwrite(cmd_line, 1, cmd_line_size, fp_out);
  fclose(fp_out);
}
