// Receives MICS-6814 JSON lines from the STM32 (via HC-05) on a Raspberry Pi.
//
// One-time pairing:
//   sudo bluetoothctl        (power on, agent on, scan on, pair <MAC>, trust <MAC>; PIN 1234 or 0000)
//   sudo rfcomm bind 0 <HC05_MAC> 1       -> creates /dev/rfcomm0
// Build: g++ -O2 -std=c++17 pi_receiver.cpp -o pi_receiver
// Run:   ./pi_receiver [/dev/rfcomm0]
// (Baud is ignored over rfcomm; the HC-05 itself must be set to 9600 to match the STM32.)

#include <cerrno>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <string>
#include <thread>

#include <fcntl.h>
#include <termios.h>
#include <unistd.h>

static int open_port(const char *path)
{
    int fd = open(path, O_RDWR | O_NOCTTY);
    if (fd < 0) {
        return -1;
    }

    termios tty{};
    if (tcgetattr(fd, &tty) == 0) {
        cfmakeraw(&tty);
        cfsetispeed(&tty, B9600);
        cfsetospeed(&tty, B9600);
        tty.c_cflag |= (CLOCAL | CREAD);
        tty.c_cc[VMIN] = 1;
        tty.c_cc[VTIME] = 0;
        tcsetattr(fd, TCSANOW, &tty);
    }
    return fd;
}

static void handle_line(const std::string &line)
{
    double co = 0, nh3 = 0, no2 = 0;
    if (std::sscanf(line.c_str(), " {\"co\":%lf,\"nh3\":%lf,\"no2\":%lf}", &co, &nh3, &no2) == 3) {
        std::printf("CO=%.2f ppm  NH3=%.2f ppm  NO2=%.2f ppm\n", co, nh3, no2);
    } else {
        std::printf("Bad line: %s\n", line.c_str());
    }
    std::fflush(stdout);
}

int main(int argc, char **argv)
{
    const char *port = (argc > 1) ? argv[1] : "/dev/rfcomm0";

    for (;;) {
        int fd = open_port(port);
        if (fd < 0) {
            std::fprintf(stderr, "Cannot open %s: %s - retrying in 3s\n", port, std::strerror(errno));
            std::this_thread::sleep_for(std::chrono::seconds(3));
            continue;
        }

        std::printf("Listening on %s\n", port);
        std::string buffer;
        char chunk[128];

        for (;;) {
            ssize_t n = read(fd, chunk, sizeof(chunk));
            if (n <= 0) {
                std::fprintf(stderr, "Link lost, reconnecting...\n");
                break;
            }
            for (ssize_t i = 0; i < n; ++i) {
                if (chunk[i] == '\n') {
                    if (!buffer.empty()) {
                        handle_line(buffer);
                    }
                    buffer.clear();
                } else if (chunk[i] != '\r') {
                    buffer.push_back(chunk[i]);
                    if (buffer.size() > 256) {
                        buffer.clear();
                    }
                }
            }
        }
        close(fd);
        std::this_thread::sleep_for(std::chrono::seconds(3));
    }
}
