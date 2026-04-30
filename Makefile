BUILD_DIR = cmake-build-debug

.PHONY: milestone1 milestone2 milestone3 clean

milestone1:
	cmake -S . -B $(BUILD_DIR)
	cmake --build $(BUILD_DIR) --target dijkstra

milestone2:
	cmake -S . -B $(BUILD_DIR)
	cmake --build $(BUILD_DIR) --target sim

milestone3:
	cmake -S . -B $(BUILD_DIR)
	cmake --build $(BUILD_DIR) --target sim

clean:
	rm -rf $(BUILD_DIR)