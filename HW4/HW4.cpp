#include <iostream>
#include <stdlib.h>  
#include <crtdbg.h>  
#include <vector>
#include <string>
#include <chrono>
#include <ctime>
#include <atomic>
#include <thread>
#include <mutex>
#include <algorithm>
#include <sstream>
#include <fstream>
#include <condition_variable>


#define UP 1;
#define DOWN 2;
#define IDLE 3;
#define MAX_FLOOR 30;
#define MIN_FLOOR 1;
#define MAX_CAPACITY 30;

using namespace std;
ostringstream logger;
ostringstream requestLogger;

mutex controlLock;
mutex logLock;
mutex coutLock;
mutex vLock;
mutex fileLock;

condition_variable cv1, cv2;
atomic<bool> flag;
atomic<int> rcount = 0;

vector<vector<char>> v; // for visualization purpose
const string log_path = "./log.txt";
const string request_log_path = "./request.txt";

class Person {
public:
	char name[64];
	int from;
	int to;

	Person(char *name, int from, int to);
	~Person();
};

class Cart {
private:
	int id;
	int capacity;
	int size;
	int currentLevel;
	int status;


	void unloadPersonAtEachLevel();

	void runCart();

	void operate();

	void updateLevel();

	void writeToFile();

public:
	mutex lock;
	bool isRunning;
	thread* t; // running thread
	vector<Person*> currentJobList;
	vector<Person*> currentWaitingList;

	Cart();

	Cart(int id, int capacity, int current_level);

	~Cart();

	void setStatus(int i);

	int getStatus();

	int getCurrentLevel();

	int getCurrentSize();

	void printElevator();
};

Cart::Cart() {
	this->id = 0;
	this->size = 0;
	this->capacity = MAX_CAPACITY;
	this->currentLevel = 1;
	this->status = IDLE;
	this->isRunning = true;
	this->t = new thread(&Cart::runCart, this);
	// t->detach();
}

Cart::Cart(int id, int capacity, int currentLevel) {
	this->id = id;
	this->size = 0;
	this->capacity = capacity;
	this->currentLevel = currentLevel;
	this->status = IDLE;
	this->isRunning = true;
	this->t = new thread(&Cart::runCart, this);
	// t->detach();
}

Cart::~Cart() = default;

Person::Person(char *name, int from, int to) {
	snprintf(this->name, sizeof(this->name), "%s", name);
	this->from = from;
	this->to = to;
}

Person::~Person() {}

ostream& operator<<(ostream & os, const vector<char> v) {
	for (auto i = v.begin(); i < v.end(); i++) {
		os << (*i);
	}
	return os;
}

ostream& operator<<(ostream & os, const vector<vector<char>> v) {
	for (auto i = v.begin(); i < v.end(); i++) {
		os << (*i) << endl;
	}
	return os;
}

ostream& operator<<(ostream & os, const vector<Person*> v) {
	for (auto i = v.begin(); i < v.end(); i++) {
		os << (*i)->name << " " << (*i)->from << " " << (*i)->to << endl;
	}
	return os;
}

void Cart::writeToFile() {
	lock.lock();
	fileLock.lock();
	ofstream m_file_stream;
	m_file_stream.open(log_path);
	if (!m_file_stream.is_open() || !m_file_stream.good()) { std::cerr << "Failed to create log.txt" << std::endl; }
	m_file_stream << logger.str() << '\n';
	m_file_stream.close();
	fileLock.unlock();
	lock.unlock();
}

void Cart::printElevator() {
	// if (status == 3) { return; }
	lock_guard<mutex> cLock(coutLock);
	lock_guard<mutex> lLock(logLock);
	lock_guard<mutex> vvLock(vLock);
	system("cls");
	if (id == 1) {
		if (currentLevel - 2 >= 0) { v[currentLevel - 2][5] = ' '; }
		if (currentLevel - 1 >= 0) { v[currentLevel - 1][5] = '*'; }
		if (currentLevel < 30) { v[currentLevel][5] = ' '; }
	}
	else if (id == 2) {
		if (currentLevel - 2 >= 0) { v[currentLevel - 2][9] = ' '; }
		if (currentLevel - 1 >= 0) { v[currentLevel - 1][9] = '*'; }
		if (currentLevel < 30) { v[currentLevel][9] = ' '; }
	}
	else if (id == 3) {
		if (currentLevel - 2 >= 0) { v[currentLevel - 2][13] = ' '; }
		if (currentLevel - 1 >= 0) { v[currentLevel - 1][13] = '*'; }
		if (currentLevel < 30) { v[currentLevel][13] = ' '; }
	}
	cout << v << endl;
	cout << "Cart " << id << " at level " << currentLevel << " is going " << (status == 1 ? "UP" : (status == 2 ? "DOWN" : "IDLE")) << endl;
	cout << "Job list " << currentJobList << endl;
	cout << "Waiting list " << currentWaitingList;
	if (status == 3) { return; }
	this_thread::sleep_for(chrono::milliseconds(500));
}

void Cart::unloadPersonAtEachLevel() {
	logLock.lock();
	for (auto i = currentJobList.begin(); i != currentJobList.end();) {
		if ((*i)->to == currentLevel) {
			logger << "Person " << (*i)->name << " arrived at " << currentLevel << endl;
			delete((*i));
			i = currentJobList.erase(i);
			this_thread::sleep_for(chrono::milliseconds(500));
			continue;
		}
		if (currentJobList.empty()) {
			this->status = IDLE;
		}
		logger << "Cart " << id << " at level " << currentLevel << " is going " << (status == 1 ? "UP" : (status == 2 ? "DOWN" : "IDLE")) << endl;
		i++;
	}
	logLock.unlock();
}

void Cart::runCart() { // dead loop in here
	while (1) {
		while (!this->currentWaitingList.empty() || !this->currentJobList.empty()) { // move from floor to floor until becomes IDLE
			lock_guard<mutex> ulock(this->lock);
			//lock.lock();
			operate(); // check available users at each level
			//lock.unlock();
			this_thread::sleep_for(chrono::milliseconds(500)); // it takes 100ms to move 1 floor
			// writeToFile();
		}
		status = IDLE;
		if (flag && this->currentWaitingList.empty() && this->currentJobList.empty()) { return; }
	}
}

void Cart::updateLevel() {
	if (status == 1) {
		if (currentLevel >= 30) { status = IDLE; }
		else {
			currentLevel++;
		}
	}
	if (status == 2) {
		if (currentLevel <= 1) { status = IDLE; }
		else { currentLevel--; }
	}
}

void Cart::operate() {
	// add users in the waiting list to current job list
	for (auto i = currentWaitingList.begin(); i != currentWaitingList.end();) {
		if ((*i)->from == currentLevel) {
			auto* p = new Person((*i)->name, (*i)->from, (*i)->to);
			if (currentJobList.empty()) {
				this->status = (((*i)->from - (*i)->to > 0) ? 2 : 1);
				currentJobList.push_back(p);
			}
			else {
				for (auto j = currentJobList.begin(); j < currentJobList.end(); j++) {
					if (p->to <= (*j)->to) { // the currentJobList is ordered by destination level ascending
						currentJobList.insert(j, p);
						break;
					}
				}
			}
			delete((*i));
			i = currentWaitingList.erase(i);
			continue;
		}
		i++;
	}
	updateLevel();
	// then check person in the currentJobList can be removed or not
	unloadPersonAtEachLevel();
	printElevator();
}

void Cart::setStatus(int i) {
	this->status = i;
}

int Cart::getStatus() {
	return this->status;
}

int Cart::getCurrentLevel() {
	return this->currentLevel;
}

int Cart::getCurrentSize() {
	return this->size;
}

class InputHandler {
private:
	Person* p;
	int count = 0;
public:
	Person* createRequestFromInput();

	InputHandler();

	~InputHandler();
};

InputHandler::InputHandler() {
	p = nullptr;
	count = 0;
}

InputHandler::~InputHandler() {
	delete (p);
}

Person* InputHandler::createRequestFromInput() {
	// coutLock.lock();
	srand(time(NULL));
	char user[64] = "user";
	sprintf_s(user, "%d", this->count);
	int fromLevel;
	int toLevel;
	fromLevel = rand() % 30 + 1;
	toLevel = rand() % 30 + 1;
	if (toLevel > 30 || toLevel < 1 || fromLevel > 30 || fromLevel < 1 || toLevel == fromLevel) {
		// cout << "Invalid request" << endl;
		return nullptr;
	}
	count++;
	auto request = new Person(user, fromLevel, toLevel);
	return request;
}

class RequestHandler {
private:
	InputHandler inputHandler;
	mutex lock;
	Cart* cartFirst;
	Cart* cartSecond;
	Cart* cartThird;

public:
	vector<Person*> requestPool;
	thread* t;
	int count;
	int dispath;
	RequestHandler();

	RequestHandler(Cart* first, Cart* second, Cart* third);

	~RequestHandler();

	void generateRequestFromInput();

	int getRequestStatus(int from, int to);

	void chooseCart();
};

RequestHandler::RequestHandler() {
	this->cartFirst = new Cart(1, 30, 1);
	this->cartSecond = new Cart(2, 30, 1);
	this->cartThird = new Cart(3, 30, 1);
	this->t = new thread(&RequestHandler::chooseCart, this);
	this->count = 0;
	this->dispath = 0;
	//this->t->detach();
}

RequestHandler::RequestHandler(Cart * first, Cart * second, Cart * third) {
	this->cartFirst = first;
	this->cartSecond = second;
	this->cartThird = third;
	this->t = new thread(&RequestHandler::chooseCart, this);
	this->count = 0;
	this->dispath = 0;
	// this->t->detach();
}

RequestHandler::~RequestHandler() {
	delete (cartFirst);
	delete (cartSecond);
	delete (cartThird);
	delete (t);
}

void RequestHandler::generateRequestFromInput() {
	lock.lock();
	Person* p = inputHandler.createRequestFromInput();
	if (p != nullptr) {
		requestLogger << p->name << " " << p->from << " " << p->to << endl;
		count++;
		requestPool.push_back(p);
	}
	this_thread::sleep_for(chrono::seconds(1));
	lock.unlock();
}

int RequestHandler::getRequestStatus(int from, int to) {
	if (from < to) { return 1; }
	else if (from > to) { return 2; }
	else { return 3; }
}

void RequestHandler::chooseCart() {
	lock.lock();
	while (!requestPool.empty()) {
		// cout << requestPool.size() << endl;
		// logger << requestPool << endl;
		// this_thread::sleep_for(chrono::milliseconds(100)); // after each schedule, sleep for 100ms
		cartFirst->lock.lock();
		cartSecond->lock.lock();
		cartThird->lock.lock();
		logLock.lock();
		int diffFirst = 999, diffSecond = 999, diffThird = 999;
		for (auto i = requestPool.begin(); i != requestPool.end();) {
			int requestStatus = getRequestStatus((*i)->from, (*i)->to);
			if (requestStatus == 3) { continue; } // skip the idle request
			if (cartFirst->getStatus() == requestStatus) {
				if (!cartFirst->currentJobList.empty()) {
					if (requestStatus == 1 && cartFirst->getCurrentLevel() <= (*i)->from) {
						diffFirst = abs((*i)->from - cartFirst->getCurrentLevel());
					}
					else if (requestStatus == 2 && cartFirst->getCurrentLevel() >= (*i)->from) {
						diffFirst = abs(cartFirst->getCurrentLevel() - (*i)->from);
					}
				}
			}

			if (cartSecond->getStatus() == requestStatus) {
				if (!cartSecond->currentJobList.empty()) {
					if (requestStatus == 1 && cartSecond->getCurrentLevel() <= (*i)->from) {
						diffSecond = abs((*i)->from - cartSecond->getCurrentLevel());
					}
					else if (requestStatus == 2 && cartSecond->getCurrentLevel() >= (*i)->from) {
						diffSecond = abs(cartSecond->getCurrentLevel() - (*i)->from);
					}
				}
			}

			if (cartThird->getStatus() == requestStatus) {
				if (!cartThird->currentJobList.empty()) {
					if (requestStatus == 1 && cartThird->getCurrentLevel() <= (*i)->from) {
						diffThird = abs((*i)->from - cartThird->getCurrentLevel());
					}
					else if (requestStatus == 2 && cartThird->getCurrentLevel() >= (*i)->from) {
						diffThird = abs(cartThird->getCurrentLevel() - (*i)->from);
					}
				}
			}

			if (cartFirst->getStatus() == 3) { diffFirst = abs(cartFirst->getCurrentLevel() - (*i)->from); }
			if (cartSecond->getStatus() == 3) { diffSecond = abs(cartSecond->getCurrentLevel() - (*i)->from); }
			if (cartThird->getStatus() == 3) { diffThird = abs(cartThird->getCurrentLevel() - (*i)->from); }

			int minDiff = min(min(diffFirst, diffSecond), diffThird); // find the closest cart
			// cout << "Mindiff is " << minDiff << endl;
			auto* request = new Person((*i)->name, (*i)->from, (*i)->to);
			if (minDiff == 999) {
				// i++;
				continue;
			}
			else if (minDiff == diffFirst && cartFirst->getCurrentSize() <= 30) {
				// add this request to the first cart
				logger << "This request " << (*i)->from << "-" << (*i)->to << " is added to the first cart " << endl;
				// r++;
				if (cartFirst->currentWaitingList.empty()) {
					cartFirst->currentWaitingList.push_back(request);
				}
				else {
					for (auto j = cartFirst->currentWaitingList.begin(); j < cartFirst->currentWaitingList.end(); j++) {
						if ((*j)->from >= request->from) {
							cartFirst->currentWaitingList.insert(j, request);
							break;
						}
					}
				}
				if (cartFirst->getStatus() == 3) { // idle
					if (cartFirst->getCurrentLevel() <= request->from) {
						cartFirst->setStatus(1); // going up
					}
					else {
						cartFirst->setStatus(2); // going down
					}
				}
			}
			else if (minDiff == diffSecond && cartSecond->getCurrentSize() <= 30) {
				// add this request to the second cart
				logger << "This request " << (*i)->from << "-" << (*i)->to << " is added to the second cart " << endl;
				// r++;
				if (cartSecond->currentWaitingList.empty()) {
					cartSecond->currentWaitingList.push_back(request);
				}
				else {
					for (auto j = cartSecond->currentWaitingList.begin(); j < cartSecond->currentWaitingList.end(); j++) {
						if ((*j)->from >= request->from) {
							cartSecond->currentWaitingList.insert(j, request);
							break;
						}
					}
				}
				if (cartSecond->getStatus() == 3) { // idle
					if (cartSecond->getCurrentLevel() <= request->from) {
						cartSecond->setStatus(1); // going up
					}
					else {
						cartSecond->setStatus(2); // going down
					}
				}
			}
			else if (minDiff == diffThird && cartThird->getCurrentSize() <= 30) {
				// add this request to the third cart
				logger << "This request " << (*i)->from << "-" << (*i)->to << " is added to the third cart " << endl;
				// r++;
				if (cartThird->currentWaitingList.empty()) {
					cartThird->currentWaitingList.push_back(request);
				}
				else {
					for (auto j = cartThird->currentWaitingList.begin(); j < cartThird->currentWaitingList.end(); j++) {
						if ((*j)->from >= request->from) {
							cartThird->currentWaitingList.insert(j, request);
							break;
						}
					}
				}
				if (cartThird->getStatus() == 3) { // idle
					if (cartThird->getCurrentLevel() <= request->from) {
						cartThird->setStatus(1); // going up
					}
					else {
						cartThird->setStatus(2); // going down
					}
				}
			}
			delete((*i));
			i = requestPool.erase(i); // finish one request
			if (requestPool.size() == 0) { break; }
		}
		logLock.unlock();
		cartFirst->lock.unlock();
		cartSecond->lock.unlock();
		cartThird->lock.unlock();
		if (requestPool.size() == 0) { break; }
	}
	lock.unlock();
}

class CartPool {
public:
	RequestHandler* requestHandler;
	Cart* cartFirst;
	Cart* cartSecond;
	Cart* cartThird;
	int count;

	CartPool();

	~CartPool();

	void printInfo();

	void start();
};

CartPool::CartPool() {
	logLock.lock();
	srand(time(NULL));
	int levelFirst = rand() % 30 + 1;
	int levelSecond = rand() % 30 + 1;
	int levelThird = rand() % 30 + 1;
	cartFirst = new Cart(1, 30, levelFirst);
	cartSecond = new Cart(2, 30, levelSecond);
	cartThird = new Cart(3, 30, levelThird);
	v[levelFirst - 1][5] = '*';
	v[levelSecond - 1][9] = '*';
	v[levelThird - 1][13] = '*';
	logger << "Cart 1 initially " << levelFirst << " Cart 2 initially " << levelSecond << " Cart 3 initially " << levelThird << endl;
	requestHandler = new RequestHandler(cartFirst, cartSecond, cartThird);
	count = 0;
	logLock.unlock();
}

CartPool::~CartPool() {
	delete (requestHandler);
	delete (cartFirst);
	delete (cartSecond);
	delete (cartThird);
}

void CartPool::printInfo() {
}

void CartPool::start() {
	requestHandler->t->join();
	while (requestHandler->count < 5) {
		requestHandler->generateRequestFromInput();
		requestHandler->chooseCart();
	}
	flag.store(true);
	cartFirst->t->join();
	cartSecond->t->join();
	cartThird->t->join();
}

int main() {
	cout << "****************** Cart simulator begins ******************" << endl;
	// vLock.lock();
	for (int i = 1; i <= 30; i++) {
		vector<char> row;
		if (i < 10) {
			row.push_back(i + '0');
			row.push_back(' ');
			row.push_back(' ');
		}
		else {
			int x = i / 10;
			int y = i % 10;
			row.push_back(x + '0');
			row.push_back(y + '0');
			row.push_back(' ');
		}
		row.push_back('|');
		row.push_back(' ');
		row.push_back(' '); // star
		row.push_back(' ');
		row.push_back('|');
		row.push_back(' ');
		row.push_back(' '); // star
		row.push_back(' ');
		row.push_back('|');
		row.push_back(' ');
		row.push_back(' '); // star
		row.push_back(' ');
		row.push_back('|');
		v.push_back(row);
	}
	// vLock.unlock();
	CartPool* cartPool = new CartPool();
	cartPool->start();
	ofstream m_file_stream;
	ofstream n_file_stream;
	m_file_stream.open(log_path);
	n_file_stream.open(request_log_path);
	if (!m_file_stream.is_open() || !m_file_stream.good() || !n_file_stream.is_open() || !n_file_stream.good()) {
		std::cerr << "Failed to create file" << std::endl;
	}
	m_file_stream << logger.str() << '\n';
	n_file_stream << requestLogger.str() << '\n';
	m_file_stream.close();
	n_file_stream.close();
	return 0;
}