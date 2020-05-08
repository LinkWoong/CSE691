// CIS600/CSE691  HW2 NetID: cwang106 SUID: 334027643
// Due: 11:59PM, Wednesday (March 4)
#include <mutex>
#include <list>
#include <iostream>
#include <string>
#include <random>
#include <vector>
#include <ctime>
#include <stdlib.h>
#include <sstream>
#include <fstream>
#include <condition_variable>
#include <chrono>
#include <thread>

using Us = std::chrono::microseconds;

std::mutex buffer_mutex;
std::mutex log_mutex;
std::mutex total_complete_product_mutex;

std::condition_variable cv_partWorker;
std::condition_variable cv_productWorker;
const std::string log_path = "./log.txt";

std::ostringstream logger;

int buffer[4] = { 0, 0, 0, 0 };
int reference[4] = { 6, 5, 4, 3 };
auto current_time_start = std::chrono::system_clock::now();

int total_complete_product = 0;

void PartWorker(int thread_num) {
	int num_iteration = 0;

	while (num_iteration < 5) {
		/* =========== start generating random load order =========== */
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
		std::lock_guard<std::mutex> log_lock(log_mutex);
		logger << "Current Time: " << start_log.count() << "us" << "\n";
		logger << "Part Worker ID: " << thread_num << "\n";
		logger << "Iteration: " << num_iteration << "\n";
		logger << "Status: New Load Order\n";
		logger << "Accumulated Waiting Time: 0us\n";
		logger << "Buffer State: (" << buffer[0] << ", " << buffer[1] << ", " << buffer[2] << ", " << buffer[3] << ")\n";
		logger << "Load Order: (" << load_order[0] << ", " << load_order[1] << ", " << load_order[2] << ", " << load_order[3] << ")\n";

		/* Time for manufacturing the parts*/
		std::this_thread::sleep_for(Us(load_order[0] * 50 + load_order[1] * 70 + load_order[2] * 90 + load_order[3] * 110));

		/* =========== start loading to buffer =========== */
		int sum = 0;
		int buffer_limit[4] = { 6, 5, 4, 3 };
		int current_sum[4] = { 0, 0, 0, 0 };
		std::unique_lock<std::mutex> ulock(buffer_mutex);
		if (buffer[0] < 6 || buffer[1] < 5 || buffer[2] < 4 || buffer[3] < 3) {
			for (int i = 0; i < 4; i++) {
				if (load_order[i] + buffer[i] <= buffer_limit[i]) {
					buffer[i] += load_order[i];
					sum += load_order[i];
					current_sum[i] = load_order[i];
					load_order[i] = 0;
				}
				else {
					int diff = buffer_limit[i] - buffer[i];
					if (diff < 0) { continue; }
					buffer[i] += diff;
					current_sum[i] = diff;
					load_order[i] -= diff;
					sum += diff;
				}
			}
			logger << "Updated Buffer State: (" << buffer[0] << ", " << buffer[1] << ", " << buffer[2] << ", " << buffer[3] << ")\n";
			logger << "Updated Load Order: (" << load_order[0] << ", " << load_order[1] << ", " << load_order[2] << ", " << load_order[3] << ")\n\n";
			std::cout << logger.str() << '\n';

			cv_productWorker.notify_one();
			std::this_thread::sleep_for(Us(current_sum[0] * 20 + current_sum[1] * 30 + current_sum[2] * 40 + current_sum[3] * 50));

			if (sum != 4) { // still remains some parts
				auto start = std::chrono::system_clock::now();
				if (cv_partWorker.wait_until(ulock, start + Us(3000), [load_order] {
					for (int i = 0; i < 4; i++) {
						if (reference[i] - buffer[i] < load_order[i]) { return false; }
					}
					return true; })) {
					// timeout
					auto end = std::chrono::system_clock::now();
					auto diff = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
					auto now_log = std::chrono::duration_cast<std::chrono::microseconds>(end - current_time_start);
					logger << "Current Time: " << now_log.count() << "us" << "\n";
					logger << "Part Worker ID: " << thread_num << "\n";
					logger << "Iteration: " << num_iteration << "\n";
					logger << "Status: Wakeup-Timeout\n";
					logger << "Accumulated Waiting Time: " << diff.count() << "us\n";
					logger << "Buffer State: (" << buffer[0] << ", " << buffer[1] << ", " << buffer[2] << ", " << buffer[3] << ")\n";
					logger << "Load Order: (" << load_order[0] << ", " << load_order[1] << ", " << load_order[2] << ", " << load_order[3] << ")\n";
					bool is_available = true;
					for (int i = 0; i < 4; i++) {
						if (reference[i] - buffer[i] < load_order[i]) {
							is_available = false;
							break;
						}
					}
					if (is_available) {
						// the buffer is able to load the remains
						for (int i = 0; i < 4; i++) {
							if (buffer[i] + load_order[i] <= buffer_limit[i]) {
								buffer[i] += load_order[i];
								load_order[i] = 0;
							}
						}
						cv_productWorker.notify_one();
						std::this_thread::sleep_for(Us(load_order[0] * 20 + load_order[1] * 30 + load_order[2] * 40 + load_order[3] * 50));
					}
					else {
						// the buffer is unable to load the remains, discard happens
						cv_productWorker.notify_one();
						std::this_thread::sleep_for(Us(load_order[0] * 20 + load_order[1] * 30 + load_order[2] * 40 + load_order[3] * 50));
						for (int i = 0; i < 4; i++) { load_order[i] = 0; }
					}
					num_iteration++;
					logger << "Updated Buffer State: (" << buffer[0] << ", " << buffer[1] << ", " << buffer[2] << ", " << buffer[3] << ")\n";
					logger << "Updated Load Order: (" << load_order[0] << ", " << load_order[1] << ", " << load_order[2] << ", " << load_order[3] << ")\n\n";
					std::cout << logger.str();
					std::cout << '\n';
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
					logger << "Buffer State: (" << buffer[0] << ", " << buffer[1] << ", " << buffer[2] << ", " << buffer[3] << ")\n";
					logger << "Load Order: (" << load_order[0] << ", " << load_order[1] << ", " << load_order[2] << ", " << load_order[3] << ")\n";
					bool is_available = true;
					for (int i = 0; i < 4; i++) {
						if (reference[i] - buffer[i] < load_order[i]) {
							is_available = false;
							break;
						}
					}
					if (is_available) {
						// the buffer is able to load the remains
						for (int i = 0; i < 4; i++) {
							if (buffer[i] + load_order[i] <= buffer_limit[i]) {
								buffer[i] += load_order[i];
								load_order[i] = 0;
							}
						}
						cv_productWorker.notify_one();
						std::this_thread::sleep_for(Us(load_order[0] * 20 + load_order[1] * 30 + load_order[2] * 40 + load_order[3] * 50));
					}
					else {
						// the buffer is unable to load the remains, discard happens
						cv_productWorker.notify_one();
						std::this_thread::sleep_for(Us(load_order[0] * 20 + load_order[1] * 30 + load_order[2] * 40 + load_order[3] * 50));
						for (int i = 0; i < 4; i++) { load_order[i] = 0; }
					}
					num_iteration++;
					logger << "Updated Buffer State: (" << buffer[0] << ", " << buffer[1] << ", " << buffer[2] << ", " << buffer[3] << ")\n";
					logger << "Updated Load Order: (" << load_order[0] << ", " << load_order[1] << ", " << load_order[2] << ", " << load_order[3] << ")\n\n";
					std::cout << logger.str();
					std::cout << '\n';

				}
			}
			else { // finished one order
				logger << '\n';
				num_iteration++;
				cv_productWorker.notify_one();
			}
		}
		else { // if initially the buffer is already full
			// wait
			logger << "Updated Buffer State: (" << buffer[0] << ", " << buffer[1] << ", " << buffer[2] << ", " << buffer[3] << ")\n";
			logger << "Updated Load Order: (" << load_order[0] << ", " << load_order[1] << ", " << load_order[2] << ", " << load_order[3] << ")\n\n";
			std::cout << logger.str() << '\n';

			auto start = std::chrono::system_clock::now();
			if (cv_partWorker.wait_until(ulock, start + Us(3000), [load_order] {
				for (int i = 0; i < 4; i++) {
					if (reference[i] - buffer[i] < load_order[i]) { return false; }
				}
				return true; })) {
				// timeout
				auto end = std::chrono::system_clock::now();
				auto diff = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
				auto now_log = std::chrono::duration_cast<std::chrono::microseconds>(end - current_time_start);
				// logger << '\n';
				logger << "Current Time: " << now_log.count() << "us" << "\n";
				logger << "Part Worker ID: " << thread_num << "\n";
				logger << "Iteration: " << num_iteration << "\n";
				logger << "Status: Wakeup-Timeout\n";
				logger << "Accumulated Waiting Time: " << diff.count() << "us\n";
				logger << "Buffer State: (" << buffer[0] << ", " << buffer[1] << ", " << buffer[2] << ", " << buffer[3] << ")\n";
				logger << "Load Order: (" << load_order[0] << ", " << load_order[1] << ", " << load_order[2] << ", " << load_order[3] << ")\n";
				bool is_available = true;
				for (int i = 0; i < 4; i++) {
					if (reference[i] - buffer[i] < load_order[i]) {
						is_available = false;
						break;
					}
				}
				if (is_available) {
					// the buffer is able to load the remains
					for (int i = 0; i < 4; i++) {
						if (buffer[i] + load_order[i] <= buffer_limit[i]) {
							buffer[i] += load_order[i];
							current_sum[i] = load_order[i];
						}
					}
					cv_productWorker.notify_one();
					std::this_thread::sleep_for(Us(current_sum[0] * 20 + current_sum[1] * 30 + current_sum[2] * 40 + current_sum[3] * 50));
				}
				else {
					// the buffer is unable to load the remains, discard happens
					cv_productWorker.notify_one();
					std::this_thread::sleep_for(Us(load_order[0] * 20 + load_order[1] * 30 + load_order[2] * 40 + load_order[3] * 50));
					for (int i = 0; i < 4; i++) { load_order[i] = 0; }
				}
				num_iteration++;
				logger << "Updated Buffer State: (" << buffer[0] << ", " << buffer[1] << ", " << buffer[2] << ", " << buffer[3] << ")\n";
				logger << "Updated Load Order: (" << load_order[0] << ", " << load_order[1] << ", " << load_order[2] << ", " << load_order[3] << ")\n\n";
				std::cout << logger.str();
				std::cout << '\n';
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
				logger << "Buffer State: (" << buffer[0] << ", " << buffer[1] << ", " << buffer[2] << ", " << buffer[3] << ")\n";
				logger << "Load Order: (" << load_order[0] << ", " << load_order[1] << ", " << load_order[2] << ", " << load_order[3] << ")\n";
				bool is_available = true;
				for (int i = 0; i < 4; i++) {
					if (reference[i] - buffer[i] < load_order[i]) {
						is_available = false;
						break;
					}
				}
				if (is_available) {
					// the buffer is able to load the remains
					for (int i = 0; i < 4; i++) {
						if (buffer[i] + load_order[i] <= buffer_limit[i]) {
							buffer[i] += load_order[i];
							load_order[i] = 0;
						}
					}
					cv_productWorker.notify_one();
					std::this_thread::sleep_for(Us(load_order[0] * 20 + load_order[1] * 30 + load_order[2] * 40 + load_order[3] * 50));
				}
				else {
					// the buffer is unable to load the remains, discard happens
					cv_productWorker.notify_one();
					std::this_thread::sleep_for(Us(load_order[0] * 20 + load_order[1] * 30 + load_order[2] * 40 + load_order[3] * 50));
					for (int i = 0; i < 4; i++) { load_order[i] = 0; }
				}
				num_iteration++;
				logger << "Updated Buffer State: (" << buffer[0] << ", " << buffer[1] << ", " << buffer[2] << ", " << buffer[3] << ")\n";
				logger << "Updated Load Order: (" << load_order[0] << ", " << load_order[1] << ", " << load_order[2] << ", " << load_order[3] << ")\n\n";
				std::cout << logger.str();
				std::cout << '\n';

			}
		}
	}
}

void ProductWorker(int thread_num) {
	int num_iteration = 0;
	while (num_iteration < 5) {
		/* generate pickup order */
		std::random_device rd;
		std::mt19937 gen(rd());
		std::uniform_int_distribution<int> dis(0, 3);
		int needs_type_3[3];
		int pickup_order[4];
		int no_need = dis(gen);
		int j = 0;
		for (int i = 0; i < 4; i++) { pickup_order[i] = 0; }
		for (int i = 0; i < 4; i++)
		{
			if (i != no_need)
				needs_type_3[j++] = i;
		}
		dis = std::uniform_int_distribution<int>(0, 2);
		int kinds = 0;

		for (int i = 0; i < 4; i++)
		{
			pickup_order[needs_type_3[dis(gen)]] += 1;
		}
		bool need_r = true;
		for (int i = 0; i < 3; i++)
		{
			if (pickup_order[needs_type_3[i]] == 0)
			{
				pickup_order[needs_type_3[i]] = 1;
				need_r = false;
				break;
			}
		}
		if (need_r)
			pickup_order[needs_type_3[dis(gen)]] += 1;

		auto startend = std::chrono::system_clock::now();
		auto start_log = std::chrono::duration_cast<std::chrono::microseconds>(startend - current_time_start);
		std::lock_guard<std::mutex> log_lock(log_mutex);
		logger << "Current Time: " << start_log.count() << "us" << "\n";
		logger << "Product Worker ID: " << thread_num << "\n";
		logger << "Iteration: " << num_iteration << "\n";
		logger << "Status: New Pickup Order\n";
		logger << "Accumulated Waiting Time: 0us\n";
		logger << "Buffer State: (" << buffer[0] << ", " << buffer[1] << ", " << buffer[2] << ", " << buffer[3] << ")\n";
		logger << "Pickup Order: (" << pickup_order[0] << ", " << pickup_order[1] << ", " << pickup_order[2] << ", " << pickup_order[3] << ")\n";

		/* =========== start picking up from the buffer =========== */
		/* Explanation:
		Basically for product worker, the logic is a mirror-image of that of part worker.
		*/

		int sum = 5;
		std::unique_lock<std::mutex> ulock(buffer_mutex);
		std::unique_lock<std::mutex> ulock2(total_complete_product_mutex);
		int pickup_num[4];
		for (int i = 0; i < 4; i++) { pickup_num[i] = 0; }
		if (buffer[0] > 0 || buffer[1] > 0 || buffer[2] > 0 || buffer[3] > 0) {
			for (int i = 0; i < 4; i++) {
				if (buffer[i] == 0) { continue; }
				if (buffer[i] - pickup_order[i] >= 0) {
					pickup_num[i] = pickup_order[i];
					buffer[i] -= pickup_order[i];
					sum -= pickup_order[i];
					pickup_order[i] = 0;
				}
				else {
					pickup_num[i] = buffer[i];
					pickup_order[i] -= buffer[i];
					sum -= buffer[i];
					buffer[i] = 0;
				}
			}
			logger << "Updated Buffer State: (" << buffer[0] << ", " << buffer[1] << ", " << buffer[2] << ", " << buffer[3] << ")\n";
			logger << "Updated Pickup Order: (" << pickup_order[0] << ", " << pickup_order[1] << ", " << pickup_order[2] << ", " << pickup_order[3] << ")\n";
			logger << "Total Completed Product: " << total_complete_product << "\n\n";
			std::cout << logger.str();
			std::cout << '\n';
			std::this_thread::sleep_for(Us(pickup_order[0] * 20 + pickup_order[1] * 30 + pickup_order[2] * 40 + pickup_order[3] * 50));

			if (sum != 0) {
				// pickup order still remains
				auto start = std::chrono::system_clock::now();
				if (cv_productWorker.wait_until(ulock, start + Us(6000), [pickup_order] {
					for (int i = 0; i < 4; i++) {
						if (buffer[i] < pickup_order[i]) { return false; }
					}
					return true; })) {
					// timeout
					auto end = std::chrono::system_clock::now();
					auto diff = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
					auto now_log = std::chrono::duration_cast<std::chrono::microseconds>(end - current_time_start);
					logger << "Current Time: " << now_log.count() << "us" << "\n";
					logger << "Product Worker ID: " << thread_num << "\n";
					logger << "Iteration: " << num_iteration << "\n";
					logger << "Status: Wakeup-Timeout\n";
					logger << "Accumulated Waiting Time: " << diff.count() << "us\n";
					logger << "Buffer State: (" << buffer[0] << ", " << buffer[1] << ", " << buffer[2] << ", " << buffer[3] << ")\n";
					logger << "Pickup Order: (" << pickup_order[0] << ", " << pickup_order[1] << ", " << pickup_order[2] << ", " << pickup_order[3] << ")\n";
					bool is_available = true;
					for (int i = 0; i < 4; i++) {
						if (buffer[i] < pickup_order[i]) {
							is_available = false;
							break;
						}
					}
					if (is_available) {
						// the buffer is able to unload the remains
						for (int i = 0; i < 4; i++) {
							if (buffer[i] == 0) { continue; }
							if (buffer[i] -= pickup_order[i] >= 0) {
								buffer[i] -= pickup_order[i];
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
						cv_partWorker.notify_one();
						std::this_thread::sleep_for(Us(pickup_order[0] * 20 + pickup_order[1] * 30 + pickup_order[2] * 40 + pickup_order[3] * 50));
						for (int i = 0; i < 4; i++) { pickup_order[i] = 0; }
					}
					num_iteration++;
					logger << "Updated Buffer State: (" << buffer[0] << ", " << buffer[1] << ", " << buffer[2] << ", " << buffer[3] << ")\n";
					logger << "Updated Pickup Order: (" << pickup_order[0] << ", " << pickup_order[1] << ", " << pickup_order[2] << ", " << pickup_order[3] << ")\n";
					logger << "Total Completed Product: " << total_complete_product << "\n\n";
					std::cout << logger.str();
					std::cout << '\n';
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
					logger << "Buffer State: (" << buffer[0] << ", " << buffer[1] << ", " << buffer[2] << ", " << buffer[3] << ")\n";
					logger << "Pickup Order: (" << pickup_order[0] << ", " << pickup_order[1] << ", " << pickup_order[2] << ", " << pickup_order[3] << ")\n";
					bool is_available = true;
					for (int i = 0; i < 4; i++) {
						if (buffer[i] < pickup_order[i]) {
							is_available = false;
							break;
						}
					}
					if (is_available) {
						// the buffer is able to unload the remains
						for (int i = 0; i < 4; i++) {
							if (buffer[i] == 0) { continue; }
							if (buffer[i] -= pickup_order[i] >= 0) {
								buffer[i] -= pickup_order[i];
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
						cv_partWorker.notify_one();
						std::this_thread::sleep_for(Us(pickup_order[0] * 20 + pickup_order[1] * 30 + pickup_order[2] * 40 + pickup_order[3] * 50));
						for (int i = 0; i < 4; i++) { pickup_order[i] = 0; }
					}
					num_iteration++;
					logger << "Updated Buffer State: (" << buffer[0] << ", " << buffer[1] << ", " << buffer[2] << ", " << buffer[3] << ")\n";
					logger << "Updated Pickup Order: (" << pickup_order[0] << ", " << pickup_order[1] << ", " << pickup_order[2] << ", " << pickup_order[3] << ")\n";
					logger << "Total Completed Product: " << total_complete_product << "\n\n";
					std::cout << logger.str();
					std::cout << '\n';
				}
			}
			else {
				// finish one pickup order
				logger << '\n';
				num_iteration++;
				total_complete_product++;
				cv_partWorker.notify_one();
			}
		}
		else {
			// buffer initially empty
			// wait for load
			logger << "Updated Buffer State: (" << buffer[0] << ", " << buffer[1] << ", " << buffer[2] << ", " << buffer[3] << ")\n";
			logger << "Updated Pickup Order: (" << pickup_order[0] << ", " << pickup_order[1] << ", " << pickup_order[2] << ", " << pickup_order[3] << ")\n";
			logger << "Total Completed Product: " << total_complete_product << "\n\n";
			std::cout << logger.str();
			std::cout << '\n';

			auto start = std::chrono::system_clock::now();
			if (cv_productWorker.wait_until(ulock, start + Us(6000), [pickup_order] {
				for (int i = 0; i < 4; i++) {
					if (buffer[i] < pickup_order[i]) { return false; }
				}
				return true; })) {
				// timeout
				auto end = std::chrono::system_clock::now();
				auto diff = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
				auto now_log = std::chrono::duration_cast<std::chrono::microseconds>(end - current_time_start);
				logger << "Current Time: " << now_log.count() << "us" << "\n";
				logger << "Product Worker ID: " << thread_num << "\n";
				logger << "Iteration: " << num_iteration << "\n";
				logger << "Status: Wakeup-Timeout\n";
				logger << "Accumulated Waiting Time: " << diff.count() << "us\n";
				logger << "Buffer State: (" << buffer[0] << ", " << buffer[1] << ", " << buffer[2] << ", " << buffer[3] << ")\n";
				logger << "Pickup Order: (" << pickup_order[0] << ", " << pickup_order[1] << ", " << pickup_order[2] << ", " << pickup_order[3] << ")\n";
				bool is_available = true;
				for (int i = 0; i < 4; i++) {
					if (buffer[i] < pickup_order[i]) {
						is_available = false;
						break;
					}
				}
				if (is_available) {
					// the buffer is able to unload the remains
					for (int i = 0; i < 4; i++) {
						if (buffer[i] == 0) { continue; }
						if (buffer[i] -= pickup_order[i] >= 0) {
							buffer[i] -= pickup_order[i];
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
					total_complete_product++;
					cv_partWorker.notify_one();
					std::this_thread::sleep_for(Us(pickup_order[0] * 20 + pickup_order[1] * 30 + pickup_order[2] * 40 + pickup_order[3] * 50));
					for (int i = 0; i < 4; i++) { pickup_order[i] = 0; }
				}
				num_iteration++;
				logger << "Updated Buffer State: (" << buffer[0] << ", " << buffer[1] << ", " << buffer[2] << ", " << buffer[3] << ")\n";
				logger << "Updated Pickup Order: (" << pickup_order[0] << ", " << pickup_order[1] << ", " << pickup_order[2] << ", " << pickup_order[3] << ")\n";
				logger << "Total Completed Product: " << total_complete_product << "\n\n";
				std::cout << logger.str();
				std::cout << '\n';
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
				logger << "Buffer State: (" << buffer[0] << ", " << buffer[1] << ", " << buffer[2] << ", " << buffer[3] << ")\n";
				logger << "Pickup Order: (" << pickup_order[0] << ", " << pickup_order[1] << ", " << pickup_order[2] << ", " << pickup_order[3] << ")\n";
				bool is_available = true;
				for (int i = 0; i < 4; i++) {
					if (buffer[i] < pickup_order[i]) {
						is_available = false;
						break;
					}
				}
				if (is_available) {
					// the buffer is able to unload the remains
					for (int i = 0; i < 4; i++) {
						if (buffer[i] == 0) { continue; }
						if (buffer[i] -= pickup_order[i] >= 0) {
							buffer[i] -= pickup_order[i];
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
					total_complete_product++;
					cv_partWorker.notify_one();
					std::this_thread::sleep_for(Us(pickup_order[0] * 20 + pickup_order[1] * 30 + pickup_order[2] * 40 + pickup_order[3] * 50));
					for (int i = 0; i < 4; i++) { pickup_order[i] = 0; }
				}
				num_iteration++;
				logger << "Updated Buffer State: (" << buffer[0] << ", " << buffer[1] << ", " << buffer[2] << ", " << buffer[3] << ")\n";
				logger << "Updated Pickup Order: (" << pickup_order[0] << ", " << pickup_order[1] << ", " << pickup_order[2] << ", " << pickup_order[3] << ")\n";
				logger << "Total Completed Product: " << total_complete_product << "\n\n";
				std::cout << logger.str();
				std::cout << '\n';
			}
		}
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