// CIS600/CSE691  HW3 NetID: cwang106 SUID: 334027643
// Due: 11:59PM, Friday (March 27)
#include <mutex>
#include <list>
#include <iostream>
#include <random>
#include <string>
#include <vector>
#include <ctime>
#include <thread>
#include <stdlib.h>
#include <sstream>
#include <fstream>
#include <condition_variable>
#include <chrono>

using Us = std::chrono::microseconds;
const std::string log_path = "./log.txt";

std::ostringstream logger;

auto current_time_start = std::chrono::system_clock::now();
int total_complete_product = 0;

class Buffer {
private:
	Buffer();
	static std::mutex buffer_mutex;
	static std::condition_variable cv_partWorker;
	static std::condition_variable cv_productWorker;

public:
	static std::mutex log_mutex;
	static std::vector<int> content;
	static const std::vector<int> reference;

	static std::vector<int> getContent();
	static std::vector<int> getReference();
	static bool checkAvailability(bool is_load);
	static void UpdateBufferPartWorker(int newContent[], std::vector<int>& current_sum, int& sum);
	static void UpdateBufferProductWorker(int newContent[], std::vector<int>& current_sum, int& sum);
	static void checkLoadRestAvailability(int load_order[], bool is_full, int& sum, int thread_num, int num_iteration);
	static void checkAssembleRestAvailability(int order[], bool is_full, int& sum, std::vector<int>& pickup_num, int thread_num, int num_iteration);
};

std::vector<int> Buffer::content = { 0, 0, 0, 0 };
const std::vector<int> Buffer::reference = { 6, 5, 4, 3 };
std::mutex Buffer::buffer_mutex;
std::mutex Buffer::log_mutex;
std::condition_variable Buffer::cv_partWorker;
std::condition_variable Buffer::cv_productWorker;

std::vector<int> Buffer::getContent() {
	return Buffer::content;
}

std::vector<int> Buffer::getReference() {
	return Buffer::reference;
}

bool Buffer::checkAvailability(bool is_load) {
	if (is_load) {
		for (int i = 0; i < 4; i++) {
			if (content[i] < reference[i]) { return true; }
		}
	}
	else {
		for (int i = 0; i < 4; i++) {
			if (content[i] > 0) { return true; }
		}
	}
	return false;
}

void Buffer::UpdateBufferPartWorker(int newContent[], std::vector<int>& current_sum, int& sum) {
	if (Buffer::checkAvailability(true)) {
		int current[] = { 0, 0, 0, 0 };
		for (int i = 0; i < 4; i++) {
			if (newContent[i] + content[i] <= reference[i]) {
				content[i] += newContent[i];
				current_sum[i] = newContent[i];
				sum += newContent[i];
				current[i] = newContent[i];
				newContent[i] = 0;
			}
			else {
				int diff = reference[i] - content[i];
				if (diff < 0) { continue; }
				content[i] += diff;
				current_sum[i] = diff;
				sum += diff;
				current[i] = diff;
				newContent[i] -= diff;
			}
		}
		cv_partWorker.notify_one();
	}
}

void Buffer::UpdateBufferProductWorker(int newContent[], std::vector<int>& current_sum, int& sum) {
	if (checkAvailability(false)) {
		for (int i = 0; i < 4; i++) {
			if (content[i] - newContent[i] >= 0) {
				content[i] -= newContent[i];
				current_sum[i] = newContent[i];
				sum -= newContent[i];
				newContent[i] = 0;
			}
			else {
				int diff = content[i];
				content[i] = 0;
				current_sum[i] = diff;
				sum -= diff;
				newContent[i] -= diff;
			}
		}
		cv_productWorker.notify_one();
	}
}

void Buffer::checkLoadRestAvailability(int load_order[], bool is_full, int& sum, int thread_num, int num_iteration) {
	std::unique_lock<std::mutex> ulock(Buffer::buffer_mutex);
	if (!is_full) {
		if (sum != 4) { // still remains some parts
			auto start = std::chrono::system_clock::now();
			if (cv_partWorker.wait_for(ulock, Us(3000)) == std::cv_status::timeout) {
				// timeout
				auto end = std::chrono::system_clock::now();
				auto diff = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
				auto now_log = std::chrono::duration_cast<std::chrono::microseconds>(end - current_time_start);
				logger << "Current Time: " << now_log.count() << "us" << "\n";
				logger << "Part Worker ID: " << thread_num << "\n";
				logger << "Iteration: " << num_iteration << "\n";
				logger << "Status: Wakeup-Timeout\n";
				logger << "Accumulated Waiting Time: " << diff.count() << "us\n";
				logger << "Buffer State: (" << content[0] << ", " << content[1] << ", " << content[2] << ", " << content[3] << ")\n";
				logger << "Load Order: (" << load_order[0] << ", " << load_order[1] << ", " << load_order[2] << ", " << load_order[3] << ")\n";
				bool is_available = true;
				for (int i = 0; i < 4; i++) {
					if (reference[i] - content[i] < load_order[i]) {
						is_available = false;
						break;
					}
				}
				if (is_available) {
					// the buffer is able to load the remains
					for (int i = 0; i < 4; i++) {
						if (content[i] + load_order[i] <= reference[i]) {
							content[i] += load_order[i];
							load_order[i] = 0;
						}
					}
					// total_complete_product++;
					cv_productWorker.notify_one();
					// std::this_thread::sleep_for(Us(load_order[0] * 20 + load_order[1] * 30 + load_order[2] * 40 + load_order[3] * 50));
				}
				else {
					// the buffer is unable to load the remains, discard happens
					cv_productWorker.notify_one();
					std::this_thread::sleep_for(Us(load_order[0] * 20 + load_order[1] * 30 + load_order[2] * 40 + load_order[3] * 50));
					for (int i = 0; i < 4; i++) { load_order[i] = 0; }
				}
				// num_iteration++;
				logger << "Updated Buffer State: (" << content[0] << ", " << content[1] << ", " << content[2] << ", " << content[3] << ")\n";
				logger << "Updated Load Order: (" << load_order[0] << ", " << load_order[1] << ", " << load_order[2] << ", " << load_order[3] << ")\n\n";
				std::cout << logger.str() << '\n';
			}
			else {
				// was waken up during the waiting
				auto end = std::chrono::system_clock::now();
				auto diff = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
				auto now_log = std::chrono::duration_cast<std::chrono::microseconds>(end - current_time_start);
				logger << "Current Time: " << now_log.count() << "us" << "\n";
				logger << "Part Worker ID: " << thread_num << "\n";
				logger << "Iteration: " << num_iteration << "\n";
				logger << "Status: Wakeup-Notified\n";
				logger << "Accumulated Waiting Time: " << diff.count() << "us\n";
				logger << "Buffer State: (" << content[0] << ", " << content[1] << ", " << content[2] << ", " << content[3] << ")\n";
				logger << "Load Order: (" << load_order[0] << ", " << load_order[1] << ", " << load_order[2] << ", " << load_order[3] << ")\n";

				// the buffer is able to load the remains
				for (int i = 0; i < 4; i++) {
					if (content[i] + load_order[i] <= reference[i]) {
						content[i] += load_order[i];
						load_order[i] = 0;
					}
				}
				cv_productWorker.notify_one();
				// std::this_thread::sleep_for(Us(load_order[0] * 20 + load_order[1] * 30 + load_order[2] * 40 + load_order[3] * 50));

				// num_iteration++;
				logger << "Updated Buffer State: (" << content[0] << ", " << content[1] << ", " << content[2] << ", " << content[3] << ")\n";
				logger << "Updated Load Order: (" << load_order[0] << ", " << load_order[1] << ", " << load_order[2] << ", " << load_order[3] << ")\n\n";
				std::cout << logger.str() << '\n';
			}
		}
		else { // finished one order
			logger << '\n';
			// num_iteration++;
			cv_productWorker.notify_one();
		}
	}
	else {
		auto start = std::chrono::system_clock::now();
		if (cv_partWorker.wait_for(ulock, Us(3000)) == std::cv_status::timeout) {
			// timeout
			auto end = std::chrono::system_clock::now();
			auto diff = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
			auto now_log = std::chrono::duration_cast<std::chrono::microseconds>(end - current_time_start);
			logger << "Current Time: " << now_log.count() << "us" << "\n";
			logger << "Part Worker ID: " << thread_num << "\n";
			logger << "Iteration: " << num_iteration << "\n";
			logger << "Status: Wakeup-Timeout\n";
			logger << "Accumulated Waiting Time: " << diff.count() << "us\n";
			logger << "Buffer State: (" << content[0] << ", " << content[1] << ", " << content[2] << ", " << content[3] << ")\n";
			logger << "Load Order: (" << load_order[0] << ", " << load_order[1] << ", " << load_order[2] << ", " << load_order[3] << ")\n";
			bool is_available = true;
			for (int i = 0; i < 4; i++) {
				if (reference[i] - content[i] < load_order[i]) {
					is_available = false;
					break;
				}
			}
			if (is_available) {
				// the buffer is able to load the remains
				for (int i = 0; i < 4; i++) {
					if (content[i] + load_order[i] <= reference[i]) {
						content[i] += load_order[i];
						load_order[i] = 0;
					}
				}
				// total_complete_product++;
				cv_productWorker.notify_one();
				// std::this_thread::sleep_for(Us(load_order[0] * 20 + load_order[1] * 30 + load_order[2] * 40 + load_order[3] * 50));
			}
			else {
				// the buffer is unable to load the remains, discard happens
				cv_productWorker.notify_one();
				std::this_thread::sleep_for(Us(load_order[0] * 20 + load_order[1] * 30 + load_order[2] * 40 + load_order[3] * 50));
				for (int i = 0; i < 4; i++) { load_order[i] = 0; }
			}
			// num_iteration++;
			logger << "Updated Buffer State: (" << content[0] << ", " << content[1] << ", " << content[2] << ", " << content[3] << ")\n";
			logger << "Updated Load Order: (" << load_order[0] << ", " << load_order[1] << ", " << load_order[2] << ", " << load_order[3] << ")\n\n";
			std::cout << logger.str() << '\n';
		}
		else {
			// was waken up during the waiting
			auto end = std::chrono::system_clock::now();
			auto diff = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
			auto now_log = std::chrono::duration_cast<std::chrono::microseconds>(end - current_time_start);
			logger << "Current Time: " << now_log.count() << "us" << "\n";
			logger << "Part Worker ID: " << thread_num << "\n";
			logger << "Iteration: " << num_iteration << "\n";
			logger << "Status: Wakeup-Notified\n";
			logger << "Accumulated Waiting Time: " << diff.count() << "us\n";
			logger << "Buffer State: (" << content[0] << ", " << content[1] << ", " << content[2] << ", " << content[3] << ")\n";
			logger << "Load Order: (" << load_order[0] << ", " << load_order[1] << ", " << load_order[2] << ", " << load_order[3] << ")\n";
			bool is_available = true;
			for (int i = 0; i < 4; i++) {
				if (reference[i] - content[i] < load_order[i]) {
					is_available = false;
					break;
				}
			}
			if (is_available) {
				// the buffer is able to load the remains
				for (int i = 0; i < 4; i++) {
					if (content[i] + load_order[i] <= reference[i]) {
						content[i] += load_order[i];
						load_order[i] = 0;
					}
				}
				// total_complete_product++;
				cv_productWorker.notify_one();
				// std::this_thread::sleep_for(Us(load_order[0] * 20 + load_order[1] * 30 + load_order[2] * 40 + load_order[3] * 50));
			}
			else {
				// the buffer is unable to load the remains, discard happens
				cv_productWorker.notify_one();
				std::this_thread::sleep_for(Us(load_order[0] * 20 + load_order[1] * 30 + load_order[2] * 40 + load_order[3] * 50));
				for (int i = 0; i < 4; i++) { load_order[i] = 0; }
			}
			// num_iteration++;
			logger << "Updated Buffer State: (" << content[0] << ", " << content[1] << ", " << content[2] << ", " << content[3] << ")\n";
			logger << "Updated Load Order: (" << load_order[0] << ", " << load_order[1] << ", " << load_order[2] << ", " << load_order[3] << ")\n\n";
			std::cout << logger.str() << '\n';

		}
	}
}

void Buffer::checkAssembleRestAvailability(int pickup_order[], bool is_full, int& sum, std::vector<int>& pickup_num, int thread_num, int num_iteration) {
	std::unique_lock<std::mutex> ulock(Buffer::buffer_mutex);
	if (!is_full) {
		if (sum != 0) {
			// pickup order still remains
			auto start = std::chrono::system_clock::now();
			if (cv_productWorker.wait_for(ulock, Us(6000)) == std::cv_status::timeout) {
				// timeout
				auto end = std::chrono::system_clock::now();
				auto diff = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
				auto now_log = std::chrono::duration_cast<std::chrono::microseconds>(end - current_time_start);
				logger << "Current Time: " << now_log.count() << "us" << "\n";
				logger << "Product Worker ID: " << thread_num << "\n";
				logger << "Iteration: " << num_iteration << "\n";
				logger << "Status: Wakeup-Timeout\n";
				logger << "Accumulated Waiting Time: " << diff.count() << "us\n";
				logger << "Buffer State: (" << content[0] << ", " << content[1] << ", " << content[2] << ", " << content[3] << ")\n";
				logger << "Pickup Order: (" << pickup_order[0] << ", " << pickup_order[1] << ", " << pickup_order[2] << ", " << pickup_order[3] << ")\n";
				bool is_available = true;
				for (int i = 0; i < 4; i++) {
					if (content[i] < pickup_order[i]) {
						is_available = false;
						break;
					}
				}
				if (is_available) {
					// the buffer is able to unload the remains
					for (int i = 0; i < 4; i++) {
						if (content[i] == 0) { continue; }
						if (content[i] -= pickup_order[i] >= 0) {
							content[i] -= pickup_order[i];
							pickup_num[i] += pickup_order[i];
							pickup_order[i] = 0;
						}
					}
					// successfully unloaded all parts, start assembling
					total_complete_product++;
					cv_partWorker.notify_one();
					std::this_thread::sleep_for(Us(pickup_num[0] * 80 + pickup_num[1] * 100 + pickup_num[2] * 120 + pickup_num[3] * 140));
				}
				else {
					// the buffer is unable to load the remains, discard happens
					std::this_thread::sleep_for(Us(pickup_order[0] * 20 + pickup_order[1] * 30 + pickup_order[2] * 40 + pickup_order[3] * 50));
					cv_partWorker.notify_one();
					for (int i = 0; i < 4; i++) { pickup_order[i] = 0; }
				}
				// num_iteration++;
				logger << "Updated Buffer State: (" << content[0] << ", " << content[1] << ", " << content[2] << ", " << content[3] << ")\n";
				logger << "Updated Pickup Order: (" << pickup_order[0] << ", " << pickup_order[1] << ", " << pickup_order[2] << ", " << pickup_order[3] << ")\n";
				logger << "Total Completed Product: " << total_complete_product << "\n\n";
				std::cout << logger.str() << '\n';
			}
			else {
				// was waken up during the waiting
				auto end = std::chrono::system_clock::now();
				auto diff = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
				auto now_log = std::chrono::duration_cast<std::chrono::microseconds>(end - current_time_start);
				logger << "Current Time: " << now_log.count() << "us" << "\n";
				logger << "Product Worker ID: " << thread_num << "\n";
				logger << "Iteration: " << num_iteration << "\n";
				logger << "Status: Wakeup-Notified\n";
				logger << "Accumulated Waiting Time: " << diff.count() << "us\n";
				logger << "Buffer State: (" << content[0] << ", " << content[1] << ", " << content[2] << ", " << content[3] << ")\n";
				logger << "Pickup Order: (" << pickup_order[0] << ", " << pickup_order[1] << ", " << pickup_order[2] << ", " << pickup_order[3] << ")\n";
				bool is_available = true;
				for (int i = 0; i < 4; i++) {
					if (content[i] < pickup_order[i]) {
						is_available = false;
						break;
					}
				}
				if (is_available) {
					// the buffer is able to unload the remains
					for (int i = 0; i < 4; i++) {
						if (content[i] == 0) { continue; }
						if (content[i] -= pickup_order[i] >= 0) {
							content[i] -= pickup_order[i];
							pickup_num[i] += pickup_order[i];
							pickup_order[i] = 0;
						}
					}
					// successfully unloaded all parts, start assembling
					total_complete_product++;
					cv_partWorker.notify_one();
					std::this_thread::sleep_for(Us(pickup_num[0] * 80 + pickup_num[1] * 100 + pickup_num[2] * 120 + pickup_num[3] * 140));
				}
				else {
					// the buffer is unable to load the remains, discard happens
					std::this_thread::sleep_for(Us(pickup_order[0] * 20 + pickup_order[1] * 30 + pickup_order[2] * 40 + pickup_order[3] * 50));
					cv_partWorker.notify_one();
					for (int i = 0; i < 4; i++) { pickup_order[i] = 0; }
				}
				logger << "Updated Buffer State: (" << content[0] << ", " << content[1] << ", " << content[2] << ", " << content[3] << ")\n";
				logger << "Updated Pickup Order: (" << pickup_order[0] << ", " << pickup_order[1] << ", " << pickup_order[2] << ", " << pickup_order[3] << ")\n";
				logger << "Total Completed Product: " << total_complete_product << "\n\n";
				std::cout << logger.str() << '\n';
			}
		}
		else {
			// finish one pickup order
			logger << '\n';
			// num_iteration++;
			total_complete_product++;
			cv_partWorker.notify_one();
		}
	}
	else {
		auto start = std::chrono::system_clock::now();
		if (cv_productWorker.wait_for(ulock, Us(6000)) == std::cv_status::timeout) {
			// timeout
			auto end = std::chrono::system_clock::now();
			auto diff = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
			auto now_log = std::chrono::duration_cast<std::chrono::microseconds>(end - current_time_start);
			logger << "Current Time: " << now_log.count() << "us" << "\n";
			logger << "Product Worker ID: " << thread_num << "\n";
			logger << "Iteration: " << num_iteration << "\n";
			logger << "Status: Wakeup-Timeout\n";
			logger << "Accumulated Waiting Time: " << diff.count() << "us\n";
			logger << "Buffer State: (" << content[0] << ", " << content[1] << ", " << content[2] << ", " << content[3] << ")\n";
			logger << "Pickup Order: (" << pickup_order[0] << ", " << pickup_order[1] << ", " << pickup_order[2] << ", " << pickup_order[3] << ")\n";
			bool is_available = true;
			for (int i = 0; i < 4; i++) {
				if (content[i] < pickup_order[i]) {
					is_available = false;
					break;
				}
			}
			if (is_available) {
				// the buffer is able to unload the remains
				for (int i = 0; i < 4; i++) {
					if (content[i] == 0) { continue; }
					if (content[i] -= pickup_order[i] >= 0) {
						content[i] -= pickup_order[i];
						pickup_num[i] += pickup_order[i];
						pickup_order[i] = 0;
					}
				}
				// successfully unloaded all parts, start assembling
				total_complete_product++;
				cv_partWorker.notify_one();
				std::this_thread::sleep_for(Us(pickup_num[0] * 80 + pickup_num[1] * 100 + pickup_num[2] * 120 + pickup_num[3] * 140));
			}
			else {
				// the buffer is unable to load the remains, discard happens
				std::this_thread::sleep_for(Us(pickup_order[0] * 20 + pickup_order[1] * 30 + pickup_order[2] * 40 + pickup_order[3] * 50));
				cv_partWorker.notify_one();
				for (int i = 0; i < 4; i++) { pickup_order[i] = 0; }
			}
			// num_iteration++;
			logger << "Updated Buffer State: (" << content[0] << ", " << content[1] << ", " << content[2] << ", " << content[3] << ")\n";
			logger << "Updated Pickup Order: (" << pickup_order[0] << ", " << pickup_order[1] << ", " << pickup_order[2] << ", " << pickup_order[3] << ")\n";
			logger << "Total Completed Product: " << total_complete_product << "\n\n";
			std::cout << logger.str() << '\n';
		}
		else {
			// was waken up during the waiting
			auto end = std::chrono::system_clock::now();
			auto diff = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
			auto now_log = std::chrono::duration_cast<std::chrono::microseconds>(end - current_time_start);
			logger << "Current Time: " << now_log.count() << "us" << "\n";
			logger << "Product Worker ID: " << thread_num << "\n";
			logger << "Iteration: " << num_iteration << "\n";
			logger << "Status: Wakeup-Notified\n";
			logger << "Accumulated Waiting Time: " << diff.count() << "us\n";
			logger << "Buffer State: (" << content[0] << ", " << content[1] << ", " << content[2] << ", " << content[3] << ")\n";
			logger << "Pickup Order: (" << pickup_order[0] << ", " << pickup_order[1] << ", " << pickup_order[2] << ", " << pickup_order[3] << ")\n";
			bool is_available = true;
			for (int i = 0; i < 4; i++) {
				if (content[i] < pickup_order[i]) {
					is_available = false;
					break;
				}
			}
			if (is_available) {
				// the buffer is able to unload the remains
				for (int i = 0; i < 4; i++) {
					if (content[i] == 0) { continue; }
					if (content[i] -= pickup_order[i] >= 0) {
						content[i] -= pickup_order[i];
						pickup_num[i] += pickup_order[i];
						pickup_order[i] = 0;
					}
				}
				// successfully unloaded all parts, start assembling
				total_complete_product++;
				cv_partWorker.notify_one();
				std::this_thread::sleep_for(Us(pickup_num[0] * 80 + pickup_num[1] * 100 + pickup_num[2] * 120 + pickup_num[3] * 140));
			}
			else {
				// the buffer is unable to load the remains, discard happens
				std::this_thread::sleep_for(Us(pickup_order[0] * 20 + pickup_order[1] * 30 + pickup_order[2] * 40 + pickup_order[3] * 50));
				cv_partWorker.notify_one();
				for (int i = 0; i < 4; i++) { pickup_order[i] = 0; }
			}
			// cv_partWorker.notify_one();
			// num_iteration++;
			logger << "Updated Buffer State: (" << content[0] << ", " << content[1] << ", " << content[2] << ", " << content[3] << ")\n";
			logger << "Updated Pickup Order: (" << pickup_order[0] << ", " << pickup_order[1] << ", " << pickup_order[2] << ", " << pickup_order[3] << ")\n";
			logger << "Total Completed Product: " << total_complete_product << "\n\n";
			std::cout << logger.str() << '\n';
		}
	}
}

void PartWorker(int thread_num) {
	int num_iteration = 0;

	while (num_iteration < 5) {
		/* generate load order */
		int load_order[4];
		for (int i = 0; i < 4; i++) { load_order[i] = 0; }

		std::random_device rd;
		std::mt19937 gen(rd());
		std::uniform_int_distribution<int> dis(0, 3);
		for (int i = 0; i < 4; i++)
			load_order[dis(gen)] += 1;

		/* System log */

		auto startend = std::chrono::system_clock::now();
		auto start_log = std::chrono::duration_cast<std::chrono::microseconds>(startend - current_time_start);
		std::lock_guard<std::mutex> log_lock(Buffer::log_mutex);
		// std::vector<int> buffer = Buffer::getContent();
		logger << "Current Time: " << start_log.count() << "us" << "\n";
		logger << "Part Worker ID: " << thread_num << "\n";
		logger << "Iteration: " << num_iteration << "\n";
		logger << "Status: New Load Order\n";
		logger << "Accumulated Waiting Time: 0us\n";
		logger << "Buffer State: (" << Buffer::content[0] << ", " << Buffer::content[1] << ", " << Buffer::content[2] << ", " << Buffer::content[3] << ")\n";
		logger << "Load Order: (" << load_order[0] << ", " << load_order[1] << ", " << load_order[2] << ", " << load_order[3] << ")\n";

		/* Time for manufacturing the parts*/
		std::this_thread::sleep_for(Us(load_order[0] * 50 + load_order[1] * 70 + load_order[2] * 90 + load_order[3] * 110));

		/* =========== start loading to buffer =========== */
		int sum = 0;

		std::vector<int> current_sum = { 0, 0, 0, 0 };
		// std::vector<int> reference = Buffer::getReference();

		if (Buffer::checkAvailability(true)) {
			Buffer::UpdateBufferPartWorker(load_order, current_sum, sum);
			logger << "Updated Buffer State: (" << Buffer::content[0] << ", " << Buffer::content[1] << ", " << Buffer::content[2] << ", " << Buffer::content[3] << ")\n";
			logger << "Updated Load Order: (" << load_order[0] << ", " << load_order[1] << ", " << load_order[2] << ", " << load_order[3] << ")\n\n";
			std::cout << logger.str() << '\n';
			// std::this_thread::sleep_for(Us(current_sum[0] * 20 + current_sum[1] * 30 + current_sum[2] * 40 + current_sum[3] * 50));
			Buffer::checkLoadRestAvailability(load_order, false, sum, thread_num, num_iteration);
		}
		else {
			logger << "Updated Buffer State: (" << Buffer::content[0] << ", " << Buffer::content[1] << ", " << Buffer::content[2] << ", " << Buffer::content[3] << ")\n";
			logger << "Updated Load Order: (" << load_order[0] << ", " << load_order[1] << ", " << load_order[2] << ", " << load_order[3] << ")\n\n";
			Buffer::checkLoadRestAvailability(load_order, true, sum, thread_num, num_iteration);
		}
		num_iteration++;
	}
}

void ProductWorker(int thread_num) {
	int num_iteration = 0;
	while (num_iteration < 5) {
		/* Generate pickup orders */
		std::random_device rd;
		std::mt19937 gen(rd());
		std::uniform_int_distribution<int> dis(0, 3);
		int needs_type_3[3];
		int pickup_order[4];
		int no_need = dis(gen);
		int j = 0;
		for (int i = 0; i < 4; i++) { pickup_order[i] = 0; }
		for (int i = 0; i < 4; i++) {
			if (i != no_need)
				needs_type_3[j++] = i;
		}
		dis = std::uniform_int_distribution<int>(0, 2);
		int kinds = 0;

		for (int i = 0; i < 4; i++) {
			pickup_order[needs_type_3[dis(gen)]] += 1;
		}
		bool need_r = true;
		for (int i = 0; i < 3; i++) {
			if (pickup_order[needs_type_3[i]] == 0)
			{
				pickup_order[needs_type_3[i]] = 1;
				need_r = false;
				break;
			}
		}
		if (need_r)
			pickup_order[needs_type_3[dis(gen)]] += 1;

		std::lock_guard<std::mutex> log_lock(Buffer::log_mutex);
		auto startend = std::chrono::system_clock::now();
		auto start_log = std::chrono::duration_cast<std::chrono::microseconds>(startend - current_time_start);
		// std::vector<int> buffer = Buffer::getContent();
		logger << "Current Time: " << start_log.count() << "us" << "\n";
		logger << "Product Worker ID: " << thread_num << "\n";
		logger << "Iteration: " << num_iteration << "\n";
		logger << "Status: New Pickup Order\n";
		logger << "Accumulated Waiting Time: 0us\n";
		logger << "Buffer State: (" << Buffer::content[0] << ", " << Buffer::content[1] << ", " << Buffer::content[2] << ", " << Buffer::content[3] << ")\n";
		logger << "Pickup Order: (" << pickup_order[0] << ", " << pickup_order[1] << ", " << pickup_order[2] << ", " << pickup_order[3] << ")\n";

		/* =========== start picking up from the buffer =========== */
		/* Explanation:
		Basically for product worker, the logic is a mirror-image of that of part worker.
		*/

		int sum = 5;
		// std::vector<int> current_sum = { 0, 0, 0, 0 };
		std::vector<int> pickup_num = { 0, 0, 0, 0 };
		if (Buffer::checkAvailability(false)) {
			Buffer::UpdateBufferProductWorker(pickup_order, pickup_num, sum);
			logger << "Updated Buffer State: (" << Buffer::content[0] << ", " << Buffer::content[1] << ", " << Buffer::content[2] << ", " << Buffer::content[3] << ")\n";
			logger << "Updated Pickup Order: (" << pickup_order[0] << ", " << pickup_order[1] << ", " << pickup_order[2] << ", " << pickup_order[3] << ")\n";
			logger << "Total Completed Product: " << total_complete_product << "\n\n";
			std::cout << logger.str() << '\n';
			// std::this_thread::sleep_for(Us(pickup_order[0] * 20 + pickup_order[1] * 30 + pickup_order[2] * 40 + pickup_order[3] * 50));
			Buffer::checkAssembleRestAvailability(pickup_order, false, sum, pickup_num, thread_num, num_iteration);
		}
		else {
			// std::cout << "pickup unavailable" << std::endl;
			logger << "Updated Buffer State: (" << Buffer::content[0] << ", " << Buffer::content[1] << ", " << Buffer::content[2] << ", " << Buffer::content[3] << ")\n";
			logger << "Updated Pickup Order: (" << pickup_order[0] << ", " << pickup_order[1] << ", " << pickup_order[2] << ", " << pickup_order[3] << ")\n";
			logger << "Total Completed Product: " << total_complete_product << "\n\n";
			Buffer::checkAssembleRestAvailability(pickup_order, true, sum, pickup_num, thread_num, num_iteration);
		}
		num_iteration++;
	}
}

int main() {

	const int m = 17, n = 16;
	std::thread partW[m];
	std::thread prodW[n];
	for (int i = 0; i < n; i++) {
		partW[i] = std::thread(PartWorker, i);
		prodW[i] = std::thread(ProductWorker, i);
	}
	for (int i = n; i < m; i++) {
		partW[i] = std::thread(PartWorker, i);
	}
	for (int i = 0; i < n; i++) {
		partW[i].join();
		prodW[i].join();
	}
	for (int i = n; i < m; i++) {
		partW[i].join();
	}
	logger << "Finish!" << std::endl;
	std::cout << logger.str() << '\n';

	/* save the log to ./log.txt, please give system permission to this program */
	std::ofstream m_file_stream;
	m_file_stream.open(log_path);
	if (!m_file_stream.is_open() || !m_file_stream.good()) { std::cerr << "Failed to create log.txt" << std::endl; }
	m_file_stream << logger.str() << '\n';
	m_file_stream.close();
	return 0;
}