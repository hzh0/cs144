#include "reassembler.hh"

#include <iostream>

using namespace std;

void insertback(std::string &str, std::deque<char>::iterator begin, std::deque<char>::iterator end) {
    while(begin != end) {
        str.push_back(*begin);
        ++begin;
    }
}

void insertback(std::deque<char> &dq, std::string::iterator begin, std::string::iterator end) {
    while(begin != end) {
        dq.push_back(*begin);
        ++begin;
    }
}

Reassembler::Reassembler(uint64_t capacity) : capacity_(capacity), first_unacceptable_index_(capacity){ }

void Reassembler::insert( uint64_t first_index, string data, bool is_last_substring, Writer& output )
{
    if (is_last_substring) last_ = true;

    capacity_ = output.available_capacity();
    first_unpopped_index_ = output.reader().bytes_popped();
    first_unassemble_index_ = output.get_len() + first_unpopped_index_;
    first_unacceptable_index_ = output.get_len() + first_unpopped_index_ + capacity_;

    if (output.is_closed() || data.empty()) {
        if (last_ && start_seg_map_.empty()) {
            output.close();
        }
        return;
    }

    if (first_index >= first_unacceptable_index_) {
        if (last_&& start_seg_map_.empty()) {
            output.close();
        }
        return;
    }

    auto end_index = first_index + data.size()-1;
    if (end_index < first_unassemble_index_) {
        if (last_&& start_seg_map_.empty()) {
            output.close();
        }
        return;
    }
    if (end_index >= first_unacceptable_index_) {
        //data.erase(data.begin() + first_unacceptable_index_-first_index, data.end());
        data = {data.begin(), data.begin() + first_unacceptable_index_-first_index};
        end_index = first_unacceptable_index_ - 1;
    }

    if (first_index < first_unassemble_index_) {
        //data.erase(data.begin(), data.begin()+ first_unassemble_index_ - first_index);
        data = {data.begin()+ first_unassemble_index_ - first_index, data.end()};
        first_index = first_unassemble_index_;
    }

    if (start_seg_map_.empty()) {
        if (first_index == first_unassemble_index_) {
            output.push(data);
            first_unassemble_index_ = first_index + data.size();
        }
        else {
            start_seg_map_.insert({{first_index, data.size()},
                                   {first_index, data.size(), {data.begin(), data.end()}}});
        }
    }
    else {
        auto it = start_seg_map_.begin();
        while(it != start_seg_map_.end()) {
            auto s = it->first.first, e = s + it->first.second - 1;
            auto &dt = it->second.data_;
            if (end_index < s-1) {
                break;
            }
            else if (first_index < s && end_index >= s-1 && end_index <= e) {
                data = data + std::string(dt.end()-(e-end_index),dt.end());
                end_index = e;
                start_seg_map_.erase(it);
                break;
            }
            else if (s <= first_index && e >= end_index) {
                return;
            }
            else if(first_index < s && end_index > e) {
                start_seg_map_.erase(it++);
            }
            else if (first_index <= e+1 && end_index > e) {
                insertback(dt, data.end()-(end_index-e),data.end());
                data = {dt.begin(), dt.end()};
                first_index = s;
                start_seg_map_.erase(it++);
            }
            else {
                ++it;
            }
        }

        start_seg_map_.insert({{first_index,data.size()},{first_index,data.size(),{data.begin(),data.end()}}});
        auto bg = start_seg_map_.begin();
        if (bg->first.first == first_unassemble_index_) {
            output.push(data);
            first_unassemble_index_ = bg->first.first + bg->first.second;
            start_seg_map_.erase(bg);
        }
    }
    if (last_ && start_seg_map_.empty() ) {
        output.close();
    }
}

uint64_t Reassembler::bytes_pending() const
{
  uint64_t ans = 0;
  for (auto &i : start_seg_map_) {
      ans += i.first.second;
  }
  return ans;
}
