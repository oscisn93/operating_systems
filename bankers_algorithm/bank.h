//
//  bank.h
//  bankers
//
//  Created by William McCarthy on 21 Mar 2022.
//

#ifndef __bank_h__
#define __bank_h__

#include "customer.h"
#include "vector_.h"
#include <iostream>
#include <mutex>
#include <sstream>

class Bank {
public:
  Bank() = default;
  Bank(const vector_<int> &available) : avail(available), customers() {}
  vector_<int> zero_vector = vector_(get_avail().size(), 0);
  vector_<int> get_avail() const { return avail; }
  bool is_avail(const vector_<int> &req) const { return req < avail; }

  bool is_safe(int id, const vector_<int> &req) {
    vector_<int> avail_copy = vector_<int>(avail);
    for (Customer *c : customers) {
      if (avail_copy - (c -> get_need()) < zero_vector) {
        return false;
      }
      avail_copy -= c -> get_need();
      avail_copy += c -> get_max();
    }
    return true;
  }

  bool req_approved(int id, const vector_<int> &req) {
    Customer *c = customers[id];
    return (!c->too_much(req) && is_safe(id, req));
  }

  void add_customer(Customer *c) { customers.push_back(c); }

  void withdraw_resources(const vector_<int> &req) {
    if (!is_avail(req)) {
      Utils::print_locked("WARNING: req: %s is not available for withdrawing\n",
                          (req.as_string().c_str()));
      return;
    }
    if (is_avail(req)) {
      avail -= req;
    }
  }
  void deposit_resources(const vector_<int> &req) { avail += req; }

  vector_<Customer *> get_customers() const { return customers; }

  void show() const {
    std::stringstream ss;
    ss << "\n+-----------------------------------------\n"
       << "|   BANK   avail: [" << avail.as_string().c_str() << "]\n"
       << "+-----------------------------------------\n";
    for (Customer *c : customers) {
      ss << "| P# " << std::setw(2) << c->get_idx() << "   "
         << (c->get_alloc().as_string().c_str()) << "   "
         << (c->get_max().as_string().c_str()) << "   "
         << (c->get_need().as_string().c_str()) << "\n";
    }
    std::string s = ss.str();
    Utils::print_locked("%s"
                        "+----------------------------------------\n",
                        (s.c_str()));
  }

  friend std::ostream &operator<<(std::ostream &os, const Bank &bank) {
    bank.show();
    return os;
  }

private:
  vector_<int> avail;
  vector_<Customer *> customers;
};

#endif /* __bank_h__ */
