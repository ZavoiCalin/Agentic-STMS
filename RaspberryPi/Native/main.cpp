#include <nlohmann/json.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <map>
#include <numeric>
#include <queue>
#include <random>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>
#include <chrono>
#include <ctime>
#include <cstdlib>
#include <sstream>
#include <iterator>
#include <cstdint>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <winhttp.h>
#else
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <termios.h>
#include <unistd.h>
#endif

namespace fs = std::filesystem;
using json = nlohmann::json;

namespace {

struct Options {
    std::string command;
    fs::path root = fs::current_path();
    std::map<std::string, std::string> values;
};

Options parse_options(int argc, char** argv) {
    if (argc < 2) {
        throw std::invalid_argument(
            "Usage: stms-native <route|traffic|train|send-route|receive-jetson|receive-bluetooth> [options]");
    }
    Options options;
    options.command = argv[1];
    for (int i = 2; i < argc; ++i) {
        const std::string argument = argv[i];
        if (argument == "--root" && i + 1 < argc) {
            options.root = fs::absolute(argv[++i]);
        } else if (argument.rfind("--", 0) == 0 && i + 1 < argc) {
            options.values[argument.substr(2)] = argv[++i];
        } else {
            throw std::invalid_argument("Unknown or incomplete argument: " + argument);
        }
    }
    return options;
}

json read_json(const fs::path& path) {
    std::ifstream input(path);
    if (!input) {
        throw std::runtime_error("Unable to open JSON file: " + path.string());
    }
    json value;
    input >> value;
    return value;
}

void write_json(const fs::path& path, const json& value) {
    std::ofstream output(path);
    if (!output) {
        throw std::runtime_error("Unable to write JSON file: " + path.string());
    }
    output << std::setw(2) << value << '\n';
}

double number_or(const json& object, const char* key, double fallback) {
    const auto it = object.find(key);
    if (it == object.end() || it->is_null()) {
        return fallback;
    }
    if (it->is_number()) {
        return it->get<double>();
    }
    if (it->is_string()) {
        try {
            return std::stod(it->get<std::string>());
        } catch (const std::exception&) {
            return fallback;
        }
    }
    return fallback;
}

std::string string_or(const json& object, const char* key, const std::string& fallback) {
    const auto it = object.find(key);
    return it != object.end() && it->is_string() ? it->get<std::string>() : fallback;
}

json normalize_air_quality(const json& raw) {
    if (!raw.is_array()) {
        throw std::runtime_error("Air-quality input must be a JSON array.");
    }
    json normalized = json::array();
    for (const auto& entry : raw) {
        if (!entry.is_object()) {
            continue;
        }
        if (entry.contains("data") && entry["data"].is_object()) {
            json row = entry["data"];
            row["timestamp"] = entry.value("timestamp", json(nullptr));
            row["sensorType"] = entry.value("sensorType", json(nullptr));
            normalized.push_back(std::move(row));
        } else {
            normalized.push_back(entry);
        }
    }
    return normalized;
}

struct TrafficRecord {
    double traffic_volume = 0;
    double average_speed = 0;
    double vehicle_count = 0;
    std::string light_status = "green";
    double rain = 0;
};

double air_penalty(const json& air) {
    return number_or(air, "co", 0) * 0.15
        + number_or(air, "no2", 0) * 0.07
        + number_or(air, "pm25", 0) * 0.03
        + number_or(air, "temperature", 25) * 0.01
        + number_or(air, "humidity", 50) * 0.01;
}

using Graph = std::map<std::string, std::map<std::string, double>>;

Graph road_graph() {
    return {
        {"Intersection A", {{"Intersection B", 5}, {"Intersection C", 10}, {"Intersection D", 8}}},
        {"Intersection B", {{"Intersection A", 5}, {"Intersection C", 4}, {"Intersection D", 7}}},
        {"Intersection C", {{"Intersection A", 10}, {"Intersection B", 4}, {"Intersection D", 6}}},
        {"Intersection D", {{"Intersection A", 8}, {"Intersection B", 7}, {"Intersection C", 6}}},
    };
}

std::vector<json> read_json_lines(const fs::path& path) {
    std::vector<json> records;
    std::ifstream input(path);
    if (!input) return records;
    std::string line;
    std::size_t line_number = 0;
    while (std::getline(input, line)) {
        ++line_number;
        if (line.empty()) continue;
        try {
            records.push_back(json::parse(line));
        } catch (const json::exception& error) {
            std::cerr << "Ignoring malformed telemetry at " << path << ':' << line_number
                      << ": " << error.what() << '\n';
        }
    }
    return records;
}

std::string intersection_for_telemetry(const json& event) {
    const std::string explicit_name = string_or(event, "intersection", "");
    if (explicit_name.rfind("Intersection ", 0) == 0) return explicit_name;
    const auto coordinates = event.find("intersection_coordinates");
    if (coordinates != event.end() && coordinates->is_object() &&
        std::abs(number_or(*coordinates, "latitude", 0) - 45.7489) < 0.0001 &&
        std::abs(number_or(*coordinates, "longitude", 0) - 21.2087) < 0.0001) {
        return "Intersection A";
    }
    const std::string camera_id = string_or(event, "camera_id", "");
    if (camera_id.size() == 4 && camera_id.rfind("Cam", 0) == 0 &&
        camera_id[3] >= '1' && camera_id[3] <= '2') {
        return "Intersection " + std::string(1, static_cast<char>('A' + camera_id[3] - '1'));
    }
    return "";
}

std::pair<std::vector<std::string>, double> shortest_route(
    const std::string& start, const std::string& end, const Graph& graph) {
    using QueueItem = std::pair<double, std::string>;
    std::map<std::string, double> distances;
    std::map<std::string, std::string> previous;
    for (const auto& [node, edges] : graph) {
        (void)edges;
        distances[node] = std::numeric_limits<double>::infinity();
    }
    if (!distances.count(start) || !distances.count(end)) {
        return {{start, end}, std::numeric_limits<double>::infinity()};
    }
    distances[start] = 0;
    std::priority_queue<QueueItem, std::vector<QueueItem>, std::greater<QueueItem>> pending;
    pending.emplace(0, start);
    while (!pending.empty()) {
        const auto [cost, node] = pending.top();
        pending.pop();
        if (cost > distances[node]) continue;
        if (node == end) break;
        for (const auto& [neighbor, weight] : graph.at(node)) {
            const double candidate = cost + weight;
            if (candidate < distances[neighbor]) {
                distances[neighbor] = candidate;
                previous[neighbor] = node;
                pending.emplace(candidate, neighbor);
            }
        }
    }
    if (!std::isfinite(distances[end])) return {{start, end}, distances[end]};
    std::vector<std::string> route{end};
    for (std::string node = end; node != start;) {
        const auto it = previous.find(node);
        if (it == previous.end()) return {{start, end}, std::numeric_limits<double>::infinity()};
        node = it->second;
        route.push_back(node);
    }
    std::reverse(route.begin(), route.end());
    return {route, distances[end]};
}

int run_route(const fs::path& root) {
    const fs::path directory = root / "RaspberryPi" / "AI_routing_algorithm";
    const fs::path air_path = directory / "sensor_data.json";
    const fs::path traffic_path = directory / "intersection_data.json";
    const json air_data = fs::exists(air_path)
        ? normalize_air_quality(read_json(air_path))
        : json::array();
    json traffic_data = fs::exists(traffic_path)
        ? read_json(traffic_path)
        : json::array();
    if (!fs::exists(air_path)) {
        std::cerr << "Air-quality history not found at " << air_path << "; using live sensor data if available.\n";
    }
    if (!fs::exists(traffic_path)) {
        std::cerr << "Traffic history not found at " << traffic_path << "; using live Jetson data if available.\n";
    }
    if (!traffic_data.is_array()) {
        throw std::runtime_error("Traffic input must be a JSON array.");
    }

    fs::path sensor_path = root / "arduino_data.jsonl";
    if (!fs::exists(sensor_path)) {
        sensor_path = root / "RaspberryPi" / "SendData" / "arduino_data.jsonl";
    }
    json latest_sensor = air_data.empty() ? json::object() : air_data.back();
    const auto sensor_events = read_json_lines(sensor_path);
    if (!sensor_events.empty() && sensor_events.back().is_object()) {
        latest_sensor = sensor_events.back();
    }
    double sensor_speed = number_or(latest_sensor, "average_speed", 0);
    if (!sensor_events.empty()) {
        sensor_speed *= 3.6;
    }

    std::map<std::string, json> latest_camera_data;
    const auto camera_events = read_json_lines(root / "jetson_data.jsonl");
    for (const auto& event : camera_events) {
        if (!event.is_object()) continue;
        const std::string intersection = intersection_for_telemetry(event);
        if (!intersection.empty()) latest_camera_data[intersection] = event;
    }
    std::map<std::string, json> live_traffic_data;
    for (const auto& [intersection, event] : latest_camera_data) {
        json row = {{"intersection", intersection},
                    {"vehicle_count", number_or(event, "vehicle_count", 0)}};
        row["traffic_volume"] = number_or(event, "traffic_volume", row["vehicle_count"].get<double>());
        row["average_speed"] = number_or(event, "average_speed",
                                         sensor_speed);
        row["light_status"] = string_or(event, "light_status", "green");
        row["rain"] = number_or(event, "rain",
                                number_or(latest_sensor, "rain", number_or(latest_sensor, "rainAnalog", 0)));
        for (const char* field : {"co", "no2", "pm25", "temperature", "humidity"}) {
            if (event.contains(field)) {
                row[field] = event[field];
            } else if (latest_sensor.contains(field)) {
                row[field] = latest_sensor[field];
            }
        }
        live_traffic_data.insert_or_assign(intersection, std::move(row));
    }

    Graph graph = road_graph();
    std::map<std::string, double> congestion;
    for (auto& [start, neighbors] : graph) {
        const json* traffic_row = nullptr;
        const auto live_it = live_traffic_data.find(start);
        if (live_it != live_traffic_data.end()) {
            traffic_row = &live_it->second;
        } else {
            const auto traffic_it = std::find_if(traffic_data.begin(), traffic_data.end(), [&](const json& row) {
                return string_or(row, "intersection", "") == start;
            });
            if (traffic_it != traffic_data.end()) traffic_row = &*traffic_it;
        }
        if (traffic_row == nullptr) {
            congestion[start] = 0;
            continue;
        }
        const TrafficRecord traffic{
            number_or(*traffic_row, "traffic_volume", 0),
            number_or(*traffic_row, "average_speed", 0),
            number_or(*traffic_row, "vehicle_count", 0),
            string_or(*traffic_row, "light_status", "green"),
            number_or(*traffic_row, "rain", 0),
        };
        const double severity = traffic.traffic_volume * 0.35
            + traffic.vehicle_count * 0.25 + air_penalty(*traffic_row)
            + (traffic.light_status == "red" ? 2.0 : 0.0)
            + traffic.rain * 0.005
            + std::max(0.0, 30.0 - traffic.average_speed) * 0.05;
        congestion[start] = std::max(0.0, severity / 10.0);
        std::cout << "Traffic at " << start << ": count=" << traffic.vehicle_count
                  << ", traffic=" << traffic.traffic_volume
                  << ", penalty=" << congestion[start] << '\n';
        for (auto& [end, distance] : neighbors) {
            (void)end;
            distance *= 1.0 + std::min(congestion[start], 3.0);
        }
    }
    const auto [route, cost] = shortest_route("Intersection A", "Intersection D", graph);
    const json results = {{"optimal_route", route}, {"total_weight", cost}};
    write_json(directory / "optimal_route_results.json", results);
    std::cout << "Route data: " << latest_camera_data.size() << " Jetson intersections"
              << ", " << sensor_events.size() << " sensor readings\n"
              << "Optimal Route: " << results["optimal_route"] << '\n'
              << "Total Weight (Cost or Time): " << cost << '\n';
    return 0;
}

int run_traffic(const fs::path& root) {
    const fs::path route_path = root / "RaspberryPi" / "AI_routing_algorithm" / "optimal_route_results.json";
    json optimal_route = json::array({"Intersection A", "Intersection D"});
    if (fs::exists(route_path)) {
        const json result = read_json(route_path);
        if (result.contains("optimal_route") && result["optimal_route"].is_array()) {
            optimal_route = result["optimal_route"];
        }
    }
    const std::size_t count = std::max<std::size_t>(optimal_route.size(), 1);
    json states = json::array();
    for (std::size_t i = 0; i < count; ++i) states.push_back("Red");
    if (!optimal_route.empty() && optimal_route[0].is_string()) {
        const std::string first = optimal_route[0].get<std::string>();
        for (std::size_t i = 0; i < count; ++i) {
            if (first == "Intersection " + std::string(1, static_cast<char>('A' + i))) {
                states[i] = "Green";
                break;
            }
        }
    }
    std::cout << "Updated Traffic Light States:\n";
    for (std::size_t i = 0; i < count; ++i) {
        std::cout << "Intersection " << static_cast<char>('A' + i) << ": " << states[i] << '\n';
    }
    write_json(root / "RaspberryPi" / "Traffic_light_commands" / "traffic_light_states.json",
               {{"traffic_light_states", states}});
    std::cout << "Traffic-light state saved locally. Azure IoT delivery is not enabled in this build.\n";
    return 0;
}

std::string utc_timestamp() {
    const std::time_t now = std::time(nullptr);
    std::tm utc{};
#ifdef _WIN32
    gmtime_s(&utc, &now);
#else
    gmtime_r(&now, &utc);
#endif
    std::ostringstream formatted;
    formatted << std::put_time(&utc, "%Y-%m-%dT%H:%M:%SZ");
    return formatted.str();
}

std::string environment_or(const char* name, const char* fallback) {
#ifdef _WIN32
    char* value = nullptr;
    std::size_t length = 0;
    if (_dupenv_s(&value, &length, name) != 0 || value == nullptr || length == 0) {
        std::free(value);
        return fallback;
    }
    std::string result(value);
    std::free(value);
    return result;
#else
    const char* value = std::getenv(name);
    return value && *value ? value : fallback;
#endif
}

struct HttpUrl {
    std::wstring host;
    std::wstring path;
    INTERNET_PORT port = INTERNET_DEFAULT_HTTP_PORT;
    bool secure = false;
};

std::wstring widen(const std::string& value) {
    return std::wstring(value.begin(), value.end());
}

HttpUrl parse_http_url(const std::string& url) {
    URL_COMPONENTS parts{};
    parts.dwStructSize = sizeof(parts);
    parts.dwHostNameLength = static_cast<DWORD>(-1);
    parts.dwUrlPathLength = static_cast<DWORD>(-1);
    parts.dwExtraInfoLength = static_cast<DWORD>(-1);
    const std::wstring wide_url = widen(url);
    if (!WinHttpCrackUrl(wide_url.c_str(), 0, 0, &parts)) {
        throw std::runtime_error("Invalid backend URL: " + url);
    }
    HttpUrl parsed;
    parsed.host.assign(parts.lpszHostName, parts.dwHostNameLength);
    parsed.path.assign(parts.lpszUrlPath, parts.dwUrlPathLength);
    if (parts.dwExtraInfoLength) {
        parsed.path.append(parts.lpszExtraInfo, parts.dwExtraInfoLength);
    }
    parsed.port = parts.nPort;
    parsed.secure = parts.nScheme == INTERNET_SCHEME_HTTPS;
    if (parts.nScheme != INTERNET_SCHEME_HTTP && parts.nScheme != INTERNET_SCHEME_HTTPS) {
        throw std::runtime_error("Backend URL must use HTTP or HTTPS.");
    }
    return parsed;
}

std::string post_json(const std::string& url, const std::string& body) {
#ifdef _WIN32
    const HttpUrl parsed = parse_http_url(url);
    HINTERNET session = WinHttpOpen(L"STMS native client/1.0",
                                    WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                    WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!session) throw std::runtime_error("Unable to initialize WinHTTP.");
    HINTERNET connection = WinHttpConnect(session, parsed.host.c_str(), parsed.port, 0);
    if (!connection) {
        WinHttpCloseHandle(session);
        throw std::runtime_error("Unable to connect to route backend.");
    }
    const DWORD flags = parsed.secure ? WINHTTP_FLAG_SECURE : 0;
    HINTERNET request = WinHttpOpenRequest(connection, L"POST", parsed.path.c_str(), nullptr,
                                           WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, flags);
    if (!request) {
        WinHttpCloseHandle(connection);
        WinHttpCloseHandle(session);
        throw std::runtime_error("Unable to create HTTP request.");
    }
    const wchar_t* headers = L"Content-Type: application/json\r\nAccept: application/json\r\n";
    const BOOL sent = WinHttpSendRequest(request, headers, static_cast<DWORD>(-1),
                                         const_cast<char*>(body.data()), static_cast<DWORD>(body.size()),
                                         static_cast<DWORD>(body.size()), 0);
    if (!sent || !WinHttpReceiveResponse(request, nullptr)) {
        WinHttpCloseHandle(request);
        WinHttpCloseHandle(connection);
        WinHttpCloseHandle(session);
        throw std::runtime_error("Route request failed: " + std::to_string(GetLastError()));
    }
    DWORD status = 0;
    DWORD status_size = sizeof(status);
    WinHttpQueryHeaders(request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                        WINHTTP_HEADER_NAME_BY_INDEX, &status, &status_size, WINHTTP_NO_HEADER_INDEX);
    std::string response;
    DWORD available = 0;
    while (WinHttpQueryDataAvailable(request, &available) && available) {
        const auto offset = response.size();
        response.resize(offset + available);
        DWORD received = 0;
        if (!WinHttpReadData(request, response.data() + offset, available, &received)) break;
        response.resize(offset + received);
    }
    WinHttpCloseHandle(request);
    WinHttpCloseHandle(connection);
    WinHttpCloseHandle(session);
    if (status < 200 || status >= 300) {
        throw std::runtime_error("Backend returned HTTP " + std::to_string(status) + ": " + response);
    }
    return response;
#else
    constexpr std::string_view prefix = "http://";
    if (url.compare(0, prefix.size(), prefix) != 0) {
        throw std::runtime_error("This build supports HTTP route URLs only.");
    }
    const auto host_start = prefix.size();
    const auto path_start = url.find('/', host_start);
    const auto authority = url.substr(host_start, path_start == std::string::npos ? std::string::npos : path_start - host_start);
    const auto colon = authority.rfind(':');
    const std::string host = colon == std::string::npos ? authority : authority.substr(0, colon);
    const std::string port = colon == std::string::npos ? "80" : authority.substr(colon + 1);
    const std::string path = path_start == std::string::npos ? "/" : url.substr(path_start);
    addrinfo hints{};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    addrinfo* addresses = nullptr;
    if (getaddrinfo(host.c_str(), port.c_str(), &hints, &addresses) != 0) {
        throw std::runtime_error("Unable to resolve backend host: " + host);
    }
    int socket_fd = -1;
    for (auto* address = addresses; address; address = address->ai_next) {
        socket_fd = static_cast<int>(socket(address->ai_family, address->ai_socktype, address->ai_protocol));
        if (socket_fd >= 0 && connect(socket_fd, address->ai_addr, address->ai_addrlen) == 0) break;
        if (socket_fd >= 0) close(socket_fd);
        socket_fd = -1;
    }
    freeaddrinfo(addresses);
    if (socket_fd < 0) throw std::runtime_error("Unable to connect to route backend.");
    const std::string request = "POST " + path + " HTTP/1.1\r\nHost: " + host +
        "\r\nContent-Type: application/json\r\nAccept: application/json\r\nConnection: close\r\nContent-Length: " +
        std::to_string(body.size()) + "\r\n\r\n" + body;
    std::size_t sent_bytes = 0;
    while (sent_bytes < request.size()) {
        const auto sent = send(socket_fd, request.data() + sent_bytes, request.size() - sent_bytes, 0);
        if (sent <= 0) {
            close(socket_fd);
            throw std::runtime_error("Unable to send route request.");
        }
        sent_bytes += static_cast<std::size_t>(sent);
    }
    std::string response;
    std::array<char, 4096> buffer{};
    for (;;) {
        const auto received = recv(socket_fd, buffer.data(), static_cast<int>(buffer.size()), 0);
        if (received <= 0) break;
        response.append(buffer.data(), static_cast<std::size_t>(received));
    }
    close(socket_fd);
    const auto line_end = response.find("\r\n");
    const auto status_start = line_end == std::string::npos ? std::string::npos : response.find(' ') + 1;
    const int status = status_start == std::string::npos
        ? 0
        : std::atoi(response.c_str() + status_start);
    if (status < 200 || status >= 300) {
        throw std::runtime_error("Route request failed: " + response);
    }
    const auto body_start = response.find("\r\n\r\n");
    return body_start == std::string::npos ? std::string{} : response.substr(body_start + 4);
#endif
}

int run_send_route() {
    const json route = json::array({
        {{"lat", 37.78825}, {"lng", -122.4324}},
        {{"lat", 37.78925}, {"lng", -122.4310}},
        {{"lat", 37.79050}, {"lng", -122.4298}},
        {{"lat", 37.79200}, {"lng", -122.4280}},
    });
    const json payload = {
        {"deviceId", environment_or("STMS_DEVICE_ID", "rpi-001")},
        {"timestamp", utc_timestamp()},
        {"status", "completed"},
        {"distanceKm", 4.6},
        {"route", route},
    };
    const std::string endpoint = environment_or("STMS_BACKEND_URL", "http://localhost:3000/api/routes");
    const std::string response = post_json(endpoint, payload.dump());
    std::cout << "Route sent successfully.\n" << response << '\n';
    return 0;
}

#ifdef _WIN32
using SocketHandle = SOCKET;
constexpr SocketHandle invalid_socket = INVALID_SOCKET;
void close_socket(SocketHandle socket) { closesocket(socket); }
#else
using SocketHandle = int;
constexpr SocketHandle invalid_socket = -1;
void close_socket(SocketHandle socket) { close(socket); }
#endif

class SocketRuntime {
public:
    SocketRuntime() {
#ifdef _WIN32
        WSADATA data{};
        if (WSAStartup(MAKEWORD(2, 2), &data) != 0) {
            throw std::runtime_error("Unable to initialize Winsock.");
        }
#endif
    }
    ~SocketRuntime() {
#ifdef _WIN32
        WSACleanup();
#endif
    }
};

int run_receive_jetson(const Options& options) {
    SocketRuntime runtime;
    const int port = options.values.count("port")
        ? std::stoi(options.values.at("port"))
        : std::stoi(environment_or("STMS_JETSON_PORT", "5000"));
    const fs::path output_path = options.values.count("output")
        ? fs::path(options.values.at("output"))
        : options.root / "jetson_data.jsonl";

    SocketHandle server = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (server == invalid_socket) throw std::runtime_error("Unable to create TCP socket.");
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    address.sin_port = htons(static_cast<unsigned short>(port));
    const int reuse = 1;
#ifdef _WIN32
    setsockopt(server, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&reuse), sizeof(reuse));
#else
    setsockopt(server, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
#endif
    if (bind(server, reinterpret_cast<sockaddr*>(&address), sizeof(address)) != 0 ||
        listen(server, 8) != 0) {
        close_socket(server);
        throw std::runtime_error("Unable to bind/listen on TCP port " + std::to_string(port));
    }
    std::cout << "Listening on port " << port << " for Jetson data...\n";
    for (;;) {
        SocketHandle client = accept(server, nullptr, nullptr);
        if (client == invalid_socket) {
            close_socket(server);
            throw std::runtime_error("TCP accept failed.");
        }
        std::string data;
        std::array<char, 4096> buffer{};
        for (;;) {
            const auto received = recv(client, buffer.data(), static_cast<int>(buffer.size()), 0);
            if (received <= 0) break;
            data.append(buffer.data(), static_cast<std::size_t>(received));
            if (data.size() > 1024 * 1024) {
                data.clear();
                break;
            }
        }
        close_socket(client);
        try {
            const json parsed = json::parse(data);
            std::ofstream output(output_path, std::ios::app);
            if (!output) throw std::runtime_error("Unable to open output file: " + output_path.string());
            output << parsed.dump() << '\n';
            output.flush();
            if (!output) throw std::runtime_error("Unable to persist Jetson telemetry: " + output_path.string());
            std::cout << "Saved Jetson data: " << parsed.dump() << '\n';
            try {
                run_route(options.root);
            } catch (const std::exception& error) {
                std::cerr << "Route recomputation after Jetson data failed: " << error.what() << '\n';
            }
        } catch (const json::exception& error) {
            std::cerr << "Invalid JSON received: " << error.what() << '\n';
        }
    }
}

int run_receive_bluetooth(const Options& options) {
    const std::string port_name = options.values.count("port")
        ? options.values.at("port")
        : environment_or("STMS_SENSOR_PORT",
#ifdef _WIN32
        "COM3"
#else
        "/dev/rfcomm0"
#endif
        );
    const fs::path output_path = options.values.count("output")
        ? fs::path(options.values.at("output"))
        : options.root / "arduino_data.jsonl";
#ifdef _WIN32
    HANDLE serial = CreateFileA(port_name.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr,
                                OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (serial == INVALID_HANDLE_VALUE) {
        throw std::runtime_error("Unable to open serial port " + port_name);
    }
    DCB settings{};
    settings.DCBlength = sizeof(settings);
    if (!GetCommState(serial, &settings)) {
        CloseHandle(serial);
        throw std::runtime_error("Unable to read serial configuration.");
    }
    settings.BaudRate = CBR_9600;
    settings.ByteSize = 8;
    settings.StopBits = ONESTOPBIT;
    settings.Parity = NOPARITY;
    if (!SetCommState(serial, &settings)) {
        CloseHandle(serial);
        throw std::runtime_error("Unable to configure serial port.");
    }
#else
    const int serial = open(port_name.c_str(), O_RDONLY | O_NOCTTY);
    if (serial < 0) throw std::runtime_error("Unable to open serial port " + port_name);
    termios settings{};
    if (tcgetattr(serial, &settings) != 0) {
        close(serial);
        throw std::runtime_error("Unable to read serial configuration.");
    }
    cfmakeraw(&settings);
    cfsetispeed(&settings, B9600);
    cfsetospeed(&settings, B9600);
    settings.c_cflag |= CLOCAL | CREAD;
    if (tcsetattr(serial, TCSANOW, &settings) != 0) {
        close(serial);
        throw std::runtime_error("Unable to configure serial port.");
    }
#endif
    std::ofstream output(output_path, std::ios::app);
    if (!output) {
#ifdef _WIN32
        CloseHandle(serial);
#else
        close(serial);
#endif
        throw std::runtime_error("Unable to open output file: " + output_path.string());
    }
    std::cout << "Receiving Arduino data on " << port_name << "...\n";
    std::string line;
    for (;;) {
        char character = 0;
#ifdef _WIN32
        DWORD read_count = 0;
        if (!ReadFile(serial, &character, 1, &read_count, nullptr)) {
            CloseHandle(serial);
            throw std::runtime_error("Serial read failed.");
        }
        if (read_count == 0) continue;
#else
        const auto read_count = read(serial, &character, 1);
        if (read_count < 0) {
            close(serial);
            throw std::runtime_error("Serial read failed.");
        }
        if (read_count == 0) continue;
#endif
        if (character != '\n') {
            if (character != '\r') line.push_back(character);
            continue;
        }
        try {
            const json parsed = json::parse(line);
            output << parsed.dump() << '\n';
            output.flush();
            std::cout << "Saved Arduino data: " << parsed.dump() << '\n';
        } catch (const json::exception& error) {
            std::cerr << "Invalid JSON from Arduino: " << error.what() << '\n';
        }
        line.clear();
    }
}

void write_npy(const fs::path& path, const std::array<std::array<double, 2>, 4>& table) {
    std::ofstream output(path, std::ios::binary);
    if (!output) throw std::runtime_error("Unable to write Q-table: " + path.string());
    const std::string header = "{'descr': '<f8', 'fortran_order': False, 'shape': (4, 2), }";
    std::string padded = header;
    const std::size_t prefix_size = 10;
    const std::size_t padding = (16 - ((prefix_size + padded.size() + 1) % 16)) % 16;
    padded.append(padding, ' ');
    padded.push_back('\n');
    output.write("\x93NUMPY", 6);
    const unsigned char version[2] = {1, 0};
    output.write(reinterpret_cast<const char*>(version), 2);
    const auto header_size = static_cast<std::uint16_t>(padded.size());
    const unsigned char length[2] = {
        static_cast<unsigned char>(header_size & 0xff),
        static_cast<unsigned char>((header_size >> 8) & 0xff),
    };
    output.write(reinterpret_cast<const char*>(length), 2);
    output.write(padded.data(), static_cast<std::streamsize>(padded.size()));
    for (const auto& row : table) {
        for (const auto value : row) {
            output.write(reinterpret_cast<const char*>(&value), sizeof(value));
        }
    }
}

int run_train(const fs::path& root) {
    std::array<std::array<double, 2>, 4> q_table{};
    std::mt19937 generator(std::random_device{}());
    std::uniform_real_distribution<double> unit(0, 1);
    std::uniform_int_distribution<int> action_distribution(0, 1);
    constexpr double alpha = 0.1;
    constexpr double gamma = 0.99;
    constexpr double epsilon = 0.1;

    for (int episode = 0; episode < 1000; ++episode) {
        const int state = 0;
        const int action = unit(generator) < epsilon
            ? action_distribution(generator)
            : (q_table[state][0] >= q_table[state][1] ? 0 : 1);
        const int next_state = action;
        const double reward = -3.0;
        const double best_next = std::max(q_table[next_state][0], q_table[next_state][1]);
        q_table[state][action] += alpha * (reward + gamma * best_next - q_table[state][action]);
        if (episode % 100 == 0) std::cout << "Episode " << episode << ", Q-table updated.\n";
    }
    write_npy(root / "q_table.npy", q_table);
    std::cout << "Q-table saved to q_table.npy\n";
    return 0;
}

} // namespace

int main(int argc, char** argv) {
    try {
        const auto options = parse_options(argc, argv);
        if (options.command == "route") return run_route(options.root);
        if (options.command == "traffic") return run_traffic(options.root);
        if (options.command == "train") return run_train(options.root);
        if (options.command == "send-route") return run_send_route();
        if (options.command == "receive-jetson") return run_receive_jetson(options);
        if (options.command == "receive-bluetooth") return run_receive_bluetooth(options);
        throw std::invalid_argument("Unknown command: " + options.command);
    } catch (const std::exception& error) {
        std::cerr << "stms-native: " << error.what() << '\n';
        return 1;
    }
}
