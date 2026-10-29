# 1. Use an official, modern C++ Linux environment
FROM gcc:latest

# 2. Set the working directory inside the cloud server
WORKDIR /app

# 3. Copy your C++ files into the server
COPY . .

# 4. Compile the code (Using Linux -pthread flag)
RUN g++ rev3.cpp -o memory_server -std=c++17 -pthread

# 5. Tell the server what to run when it boots up
CMD ["./memory_server"]