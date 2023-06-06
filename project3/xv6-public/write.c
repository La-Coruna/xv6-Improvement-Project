#include "types.h"
#include "stat.h"
#include "user.h"
#include "fcntl.h"

#define BUFFER_SIZE 512

int main(int argc, char *argv[]) {
  int fd, n;
  char buffer[BUFFER_SIZE];

  if (argc != 2) {
    printf(2, "Usage: fileio <filename>\n");
    exit();
  }

  // 파일 열기
  if ((fd = open(argv[1], O_WRONLY | O_CREATE)) < 0) {
    printf(2, "Failed to open file %s\n", argv[1]);
    exit();
  }

  printf(1, "Enter text (Ctrl+D to finish):\n");

  // 사용자로부터 텍스트 입력 받기
  while ((n = read(0, buffer, sizeof(buffer))) > 0) {
    // 파일에 입력한 텍스트 출력
    if (write(fd, buffer, n) != n) {
      printf(2, "Failed to write to file\n");
      exit();
    }
  }

  // 파일 닫기
  close(fd);
  exit();
}
