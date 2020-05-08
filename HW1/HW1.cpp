//CIS600/CSE691  HW1 NetID: cwang106 SUID: 334027643
//Due: 11:59PM, Sunday (February 2)

/*
Implement the two member functions: merge_sort and merge, as defined below for a sequential merge sort.
Note that the merge will be called by merge_sort.

In implementing both functions, you are only allowed to modify "next" and "previous" of nodes, but not "values" of nodes. 
You are not allowed to use any external structures such as array, linked list, etc.
You are not allowed to create any new node.
You are not allowed to create any new function.


After completing the above sequential version,  create a parallel version, by using two additional threads to speed up the merge sort.
You have to use the two functions you have implemented above.  You are not allowed to create new functions. Extra work will be needed in main function.

In your threaded implementation, you are allowed to introduce an extra node and a global pointer to the node.

It is alright if your implementation does not require the extra node or global pointer to node.

*/

#include <iostream>
#include <thread>

using namespace std;


class node {
public:
	int value;
	node * next;
	node * previous;
	node(int i) { value = i; next = previous = nullptr; }
	node() { next = previous = nullptr; }
};

class doubly_linked_list {
public:
	int num_nodes;
	node * head;
	node * tail;
	doubly_linked_list() { num_nodes = 0; head = tail = nullptr; }
	void make_random_list(int m, int n);
	void print_forward();
	void print_backward();


	//Recursively merge sort i numbers starting at node pointed by p
	void merge_sort(node * p, int i);//in-place recursive merge sort


	//Merge i1 numbers starting at node pointed by p1 with i2 numbers
	//starting at node pointed by p2
	void merge(node * p1, int i1, node * p2, int i2);

	

};

void doubly_linked_list::make_random_list(int m, int n) {

	for (int i = 0; i < m; i++) {
		node * p1 = new node(rand() % n);
		p1->previous = tail;
		if (tail != nullptr) tail->next = p1;
		tail = p1;
		if (head == nullptr) head = p1;
		num_nodes++;
	}
}

void doubly_linked_list::print_forward() {
	cout << endl;
	node * p1 = head;
	while (p1 != nullptr) {
		cout << p1->value << " ";
		p1 = p1->next;
	}
}

void doubly_linked_list::print_backward() {
	cout << endl;
	node * p1 = tail;
	while (p1 != nullptr) {
		cout << p1->value << " ";
		p1 = p1->previous;
	}
}

void doubly_linked_list::merge_sort(node *p, int i) {
    if (i < 2) { return; }
    int i1 = i / 2, i2 = i - i1;
    node *p1 = p, *p2 = p, *prev = p;

    for (int j = 0; j < i1; j++) {
        p2 = p2->next;
    }

    for (int j = 0; j < i1 - 1; j++) {
        prev = prev->next;
    }

    if (prev) { prev->next = nullptr; }
    if (p2) { p2->previous = nullptr; }
    merge_sort(p1, i1);
    while (p1 && p1->previous) {
        p1 = p1->previous;
    }
    merge_sort(p2, i2);
    while (p2 && p2->previous) {
        p2 = p2->previous;
    }
    merge(p1, i1, p2, i2);
}

void doubly_linked_list::merge(node *p1, int i1, node *p2, int i2) {
    if (i1 == 0 && i2 == 0) { return; }
    node *temp = nullptr;
    node *current1 = p1, *current2 = p2;

    if (p1 && p2) {
        if (p1->value > p2->value) {
            temp = p2;
            node *next = current2->next;
            current2 = next;
            i2--;
        } else {
            temp = p1;
            node *next = current1->next;
            current1 = next;
            i1--;
        }
    }

    while (i1 && i2) {
        if (!current1 || !current2) { break; }
        if (current1->value < current2->value) {
            temp->next = current1;
            current1->previous = temp;
            node *next = current1->next ? current1->next : nullptr;
            // if (next) { next->previous = nullptr; }
            current1 = next;
            i1--;
        } else {
            temp->next = current2;
            current2->previous = temp;
            node *next = current2->next ? current2->next : nullptr;
            // if (next) { next->previous = nullptr; }
            current2 = next;
            i2--;
        }
        // head = head->value > temp->value ? temp : head;
        temp = temp->next;
    }

    temp->next = i1 <= 0 ? current2 : current1;

    if (current1) { current1->previous = temp; }
    if (current2) { current2->previous = temp; }

    node *newHead = temp, *newTail = temp;
    while (newHead && newHead->previous) {
        newHead = newHead->previous;
    }
    while (newTail && newTail->next) {
        newTail = newTail->next;
    }
    head = newHead;
    tail = newTail;
}



int main() {
	/*
    Implement the merge_sort and merge_functions defined above to complete a sequential version of
    merge sort.
     */
    doubly_linked_list d1, d2;
    d1.make_random_list(30, 20);
    d1.print_forward();
    d1.print_backward();

    d1.merge_sort(d1.head, d1.num_nodes);
    d1.print_forward();
    d1.print_backward();


    d2.make_random_list(50, 40);
    d2.print_forward();
    d2.print_backward();
    /*
    Create two additional threads to speed up the merge sort.
    You have to still use the same merge_sort and merge functions implemented above.
    You will need to do some extra work within main function.
    */

    int i1 = d2.num_nodes / 2, i2 = d2.num_nodes - i1;
    node *p1 = d2.head, *p2 = d2.head, *prev = d2.head;

    for (int j = 0; j < i1; j++) {
        p2 = p2->next;
    }

    for (int j = 0; j < i1 - 1; j++) {
        prev = prev->next;
    }

    if (prev) { prev->next = nullptr; }
    if (p2) { p2->previous = nullptr; }

    thread t1 {&doubly_linked_list::merge_sort, d2, p1, i1};
    node *pp1 = p1;
    while (pp1 && pp1->next) {
        pp1 = pp1->next;
    }
    thread t2 {&doubly_linked_list::merge_sort, d2, p2, i2};
    node *pp2 = p2;
    while (pp2 && pp2->previous) {
        pp2 = pp2->previous;
    }

    if (t1.joinable()) { t1.join(); }
    if (t2.joinable()) { t2.join(); }

    node *h1 = pp1, *h2 = pp2;
    while (h1 && h1->previous) { h1 = h1->previous; }
    while (h2 && h2->previous) { h2 = h2->previous; }

    d2.merge(h1, i1, h2, i2);

    d2.print_forward();
    d2.print_backward();

    return 0;

}