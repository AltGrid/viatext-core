#include <iostream>
#include <vector>
#include <fcntl.h>
#include <unistd.h>
#include "CLI11.hpp"
#include "slip.hpp" // from earlier

enum Verb : uint8_t { GET_ID=0x01, SET_ID=0x02, PING=0x03 };

std::vector<uint8_t> build_frame(uint8_t verb, uint8_t seq) {
    std::vector<uint8_t> buf;
    buf.push_back(verb);
    buf.push_back(0); // flags
    buf.push_back(seq);
    buf.push_back(0); // TLV len
    return buf;
}

int main(int argc, char** argv) {
    CLI::App app{"ViaText CLI skeleton"};
    bool get_id=false, ping=false;
    std::string set_id;

    app.add_flag("--get-id", get_id);
    app.add_flag("--ping", ping);
    app.add_option("--set-id", set_id);

    std::string dev="/dev/ttyACM0";
    app.add_option("--dev", dev);

    CLI11_PARSE(app, argc, argv);

    int fd = open(dev.c_str(), O_RDWR | O_NOCTTY);
    if (fd<0) { perror("open"); return 1; }

    uint8_t seq = 1;
    std::vector<uint8_t> frame;
    if (get_id) frame = build_frame(GET_ID, seq++);
    else if (ping) frame = build_frame(PING, seq++);
    else if (!set_id.empty()) frame = build_frame(SET_ID, seq++);

    std::vector<uint8_t> out;
    viatext::slip::encode(frame.data(), frame.size(), out);
    write(fd, out.data(), out.size());

    // read one frame back
    viatext::slip::decoder dec;
    std::vector<uint8_t> resp;
    uint8_t b;
    while (read(fd, &b, 1) == 1) {
        if (dec.feed(b, resp)) {
            std::cout << "Got frame (" << resp.size() << " bytes):";
            for (auto x : resp) std::cout << " " << std::hex << (int)x;
            std::cout << std::dec << "\n";
            break;
        }
    }
    close(fd);
}
