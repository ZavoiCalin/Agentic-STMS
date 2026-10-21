#include <iostream>
#include <string>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cuda_runtime.h>

// CUDA kernel: fills an array with some values
__global__ void fillData(int* data, int size) {
    int idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx < size) {
        data[idx] = idx * 2;  // example calculation
    }
}

int main() {
    // 1. CUDA computation
    const int size = 10;
    int *d_data;
    int h_data[size];

    cudaMalloc(&d_data, size * sizeof(int));
    fillData<<<1, size>>>(d_data, size);
    cudaMemcpy(h_data, d_data, size * sizeof(int), cudaMemcpyDeviceToHost);
    cudaFree(d_data);

    // Create a JSON string with CUDA results (simple manual formatting)
    std::string json_data = "{\"cuda_results\": [";
    for (int i = 0; i < size; i++) {
        json_data += std::to_string(h_data[i]);
        if (i != size - 1) json_data += ", ";
    }
    json_data += "]}";

    // 2. TCP socket client code to send json_data
    const char* server_ip = "RASPBERRY_PI_IP";  // Replace with Pi IP
    int server_port = 5000;

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        std::cerr << "Socket creation error\n";
        return -1;
    }

    struct sockaddr_in serv_addr;
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(server_port);

    if (inet_pton(AF_INET, server_ip, &serv_addr.sin_addr) <= 0) {
        std::cerr << "Invalid address/ Address not supported\n";
        return -1;
    }

    if (connect(sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
        std::cerr << "Connection Failed\n";
        return -1;
    }

    send(sock, json_data.c_str(), json_data.length(), 0);
    std::cout << "Sent JSON data: " << json_data << "\n";

    close(sock);
    return 0;
}
