#include <string>
#include <vector>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>

const char PROMPT[] = "Введите имя файла: ";
const char E_READ_FILENAME[] = "Ошибка чтения имени файла\n";
const char E_EMPTY_FILENAME[] = "Пустое имя файла\n";
const char E_PIPE1[] = "pipe1 error\n";
const char E_PIPE2[] = "pipe2 error\n";
const char E_FORK[] = "fork error\n";
const char E_DUP2_STDIN[] = "child(before exec): dup2 stdin failed\n";
const char E_DUP2_STDERR[] = "child(before exec): dup2 stderr failed\n";
const char E_EXECL[] = "execl failed\n";

static ssize_t SafeWrite(int fd, const char *buf, size_t count) {
    size_t written = 0;
    while (written < count) {
        ssize_t w = write(fd, buf + written, count - written);
        if (w < 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        written += (size_t)w;
    }
    return (ssize_t)written;
}

static ssize_t SafeRead(int fd, char *buf, size_t count) {
    while (1) {
        ssize_t r = read(fd, buf, count);
        if (r < 0) {
            if (errno == EINTR) continue;
        }
        return r;
    }
}

int main() {
    // Считываем имя файла
    if (SafeWrite(STDOUT_FILENO, PROMPT, sizeof(PROMPT) - 1) < 0) {
        return 1;
    }

    char tmpbuf[512];
    ssize_t rn = SafeRead(STDIN_FILENO, tmpbuf, sizeof(tmpbuf));
    if (rn <= 0) {
        SafeWrite(STDOUT_FILENO, E_READ_FILENAME, sizeof(E_READ_FILENAME) - 1);
        return 1;
    }

    size_t fnLen = (size_t)rn;
    while (fnLen > 0 && (tmpbuf[fnLen - 1] == '\n' || tmpbuf[fnLen - 1] == '\r')) fnLen--;
    std::string filename(tmpbuf, tmpbuf + fnLen);

    if (filename.empty()) {
        SafeWrite(STDOUT_FILENO, E_EMPTY_FILENAME, sizeof(E_EMPTY_FILENAME) - 1);
        return 1;
    }

    int pipe1[2]; // parent -> child
    int pipe2[2]; // child -> parent (errors)

    if (pipe(pipe1) == -1) {
        SafeWrite(STDOUT_FILENO, E_PIPE1, sizeof(E_PIPE1) - 1);
        return 1;
    }
    if (pipe(pipe2) == -1) {
        SafeWrite(STDOUT_FILENO, E_PIPE2, sizeof(E_PIPE2) - 1);
        close(pipe1[0]); close(pipe1[1]);
        return 1;
    }

    pid_t pid = fork();
    if (pid == -1) {
        SafeWrite(STDOUT_FILENO, E_FORK, sizeof(E_FORK) - 1);
        close(pipe1[0]); close(pipe1[1]);
        close(pipe2[0]); close(pipe2[1]);
        return 1;
    }

    if (pid == 0) {
        // === дочерний (до exec) ===
        close(pipe1[1]);
        close(pipe2[0]);

        if (dup2(pipe1[0], STDIN_FILENO) == -1) {
            SafeWrite(STDERR_FILENO, E_DUP2_STDIN, sizeof(E_DUP2_STDIN) - 1);
            _exit(1);
        }
        if (dup2(pipe2[1], STDERR_FILENO) == -1) {
            SafeWrite(STDERR_FILENO, E_DUP2_STDERR, sizeof(E_DUP2_STDERR) - 1);
            _exit(1);
        }

        close(pipe1[0]);
        close(pipe2[1]);

        execl("./child", "child", filename.c_str(), (char*)nullptr);

        SafeWrite(STDERR_FILENO, E_EXECL, sizeof(E_EXECL) - 1);
        _exit(1);
    } else {
        // === родитель ===
        close(pipe1[0]);
        close(pipe2[1]);

        std::vector<char> buf(4096);
        std::string acc;
        ssize_t r;
        while ((r = SafeRead(STDIN_FILENO, buf.data(), buf.size())) > 0) {
            acc.append(buf.data(), buf.data() + r);
            size_t pos;
            while ((pos = acc.find('\n')) != std::string::npos) {
                std::string line = acc.substr(0, pos + 1);
                acc.erase(0, pos + 1);
                SafeWrite(pipe1[1], line.c_str(), line.size());
            }
        }
        if (!acc.empty()) {
            acc.push_back('\n');
            SafeWrite(pipe1[1], acc.c_str(), acc.size());
        }
        close(pipe1[1]);

        while ((r = SafeRead(pipe2[0], buf.data(), buf.size())) > 0) {
            SafeWrite(STDOUT_FILENO, buf.data(), (size_t)r);
        }
        close(pipe2[0]);

        int status = 0;
        waitpid(pid, &status, 0);
    }

    return 0;
}
