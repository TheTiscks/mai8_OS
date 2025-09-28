#include <string>
#include <vector>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>

const int FILE_PERMISSIONS = 0644;
const char E_MISSING_FILENAME[] = "child: missing filename\n";
const char E_OPEN_FAILED[] = "child: open failed: ";
const char E_DUP2_FAILED[] = "child: dup2 file->stdout failed: ";
const char E_RULE_ERROR[] = "Error: line must end with '.' or ';'\n";

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

int main(int argc, char *argv[]) {
    if (argc < 2) {
        SafeWrite(STDERR_FILENO, E_MISSING_FILENAME, sizeof(E_MISSING_FILENAME) - 1);
        return 1;
    }
    const char *filename = argv[1];

    int fileFd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, FILE_PERMISSIONS);
    if (fileFd < 0) {
        std::string err = E_OPEN_FAILED;
        err += strerror(errno);
        err.push_back('\n');
        SafeWrite(STDERR_FILENO, err.c_str(), err.size());
        return 1;
    }

    if (dup2(fileFd, STDOUT_FILENO) == -1) {
        std::string err = E_DUP2_FAILED;
        err += strerror(errno);
        err.push_back('\n');
        SafeWrite(STDERR_FILENO, err.c_str(), err.size());
        close(fileFd);
        return 1;
    }
    close(fileFd);

    std::vector<char> buf(4096);
    std::string acc;
    ssize_t r;
    while ((r = SafeRead(STDIN_FILENO, buf.data(), buf.size())) > 0) {
        acc.append(buf.data(), buf.data() + r);
        size_t pos;
        while ((pos = acc.find('\n')) != std::string::npos) {
            std::string line = acc.substr(0, pos);
            acc.erase(0, pos + 1);

            if (!line.empty() && (line.back() == '.' || line.back() == ';')) {
                line.push_back('\n');
                SafeWrite(STDOUT_FILENO, line.c_str(), line.size());
            } else {
                SafeWrite(STDERR_FILENO, E_RULE_ERROR, sizeof(E_RULE_ERROR) - 1);
            }
        }
    }

    if (!acc.empty()) {
        if (!acc.empty() && (acc.back() == '.' || acc.back() == ';')) {
            acc.push_back('\n');
            SafeWrite(STDOUT_FILENO, acc.c_str(), acc.size());
        } else {
            SafeWrite(STDERR_FILENO, E_RULE_ERROR, sizeof(E_RULE_ERROR) - 1);
        }
    }

    return 0;
}
