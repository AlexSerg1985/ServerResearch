#ifndef NONBLOCKSTRUCTURE_H
#define NONBLOCKSTRUCTURE_H
/*
 * Software License Agreement (BSD License)
 *
 * Copyright (c) 2017, Aleksandr Sergeevich
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *      * Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *      * Redistributions in binary form must reproduce the above copyright
 *      notice, this list of conditions and the following disclaimer in the
 *      documentation and/or other materials provided with the distribution.
 *      * Neither the name of the Bauman Moscow State Technical University,
 *      nor the names of its contributors may be used to endorse or promote
 *      products derived from this software without specific prior written
 *      permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

class StatTool {
public:
    StatTool() {}
    void setSWTime() {
        WStime = boost::posix_time::microsec_clock::universal_time();
    }
    void setSRTime() {
        RStime = boost::posix_time::microsec_clock::universal_time();
    }
    void setCount(unsigned int& size_) {
        size = size_;
    }
    void calcWRate(unsigned int& count) {
        boost::posix_time::ptime now = boost::posix_time::microsec_clock::universal_time();
        Wd = WStime - now;
    }
    void calcRRate(unsigned int& count) {
        boost::posix_time::ptime now = boost::posix_time::microsec_clock::universal_time();
        Rd = RStime - now;
    }
    unsigned int getOptSize() {
        double rate = 1.0;
        if(Rd < Wd) rate = Wd.total_microseconds()/Rd.total_microseconds();
        return std::floor(rate*size);
    }
private:
    boost::posix_time::ptime RStime;
    boost::posix_time::ptime WStime;
    boost::posix_time::time_duration Rd;
    boost::posix_time::time_duration Wd;
    unsigned int size;
    double rps;
    double wps;
    double rate;
};

template<class T>
class simpleRing {
public:
    simpleRing(int capacity_) {
        flag = false;
        this->capacity = capacity_;
        flipped = false;
        writePos = readPos = 0;
        this->buffer = new T[capacity];
    }
    bool push(const T& element) {
        calcPutPos();
        this->buffer[writePos] = element;
        return true;
    }
    void calcPutPos() {
        if(writePos == capacity-1) {
            writePos = 0;
            flipped = true;
        }
        else {
            ++writePos;
        }
    }
    unsigned int remainingCapacity() {
        if(!flipped) {
            return capacity - writePos;
        }
        else {
            std::cout << "WPos: " << writePos << "--" << "RPos: " << readPos << std::endl;
        }
        return readPos - writePos;
    }
    unsigned int getCapacity() {
        return capacity;
    }
    unsigned int available() {
        if(!flipped) {
            return writePos - readPos;
        }
        return capacity - readPos + writePos;
    }
    bool deleteRing() {
        return flag;
    }
    void setFlag(bool flag_) {
        this->flag = flag_;
    }
    T take() {
        if(!flipped) {
            if(readPos < writePos) {
                return buffer[readPos++];
            }
            else {
                return NULL;
            }
        }
        else {
            if(readPos == capacity) {
                readPos = 0;
                flipped = false;
                if(readPos < writePos) {
                    return buffer[readPos++];
                }
                else {
                    return NULL;
                }
            }
            else {
                return buffer[readPos++];
            }
        }
    }
private:
    boost::atomic<bool> flag;
    unsigned int capacity = 100;
    boost::atomic<unsigned int> writePos;
    boost::atomic<unsigned int> readPos;
    boost::atomic<bool> flipped;
    T* buffer = NULL;
};

template<class T, int gap = 5>
class RingBufferFlip4 {
public:
    RingBufferFlip4(const int& capacity_) {
        permit = true;
        this->capacity = capacity_;
        buffer.insert( std::pair<int, simpleRing<T>*>(0, new simpleRing<T>(10)) );
        resetAll();
    }
    void reset() {
        this->writePos = 0;
        this->readPos  = 0;
        this->flipped  = false;
    }
    void resetAll() {
        this->writePos = 0;
        this->readPos  = 0;
        this->flipped  = false;
        this->permit = true;
        this->number = 0;
        this->numFuture = 0;
        this->numEpoche = 0;
    }
    int available() {
        if(!flipped) {
            return writePos - readPos;
        }
        return capacity - readPos + writePos;
    }
    bool push(const T& element) {
        while(!permit);
        this->permitDel = false;
        typename std::map<int, simpleRing<T>*>::iterator wit = this->buffer.begin();
        int index = wit->first;
        simpleRing<T>* ring = wit->second;
        this->permitDel = true;
        if(ring->remainingCapacity() > 0) {
            ring->push(element);
            std::cout << "Ring " << index << " capacity equal to " << ring->remainingCapacity() << std::endl;
        }
        else {
            ring->setFlag(true);
            unsigned int capacity = 1.5 * ring->getCapacity();
            simpleRing<T>* ring = new simpleRing<T>(capacity);
            std::cout << "Add new ring with index " << ++index << " and capacity " << capacity << std::endl;
            buffer.insert( std::pair<int, simpleRing<T>*>(index, ring) );
            ring->push(element);
            std::cout << "Ring " << index << " capacity equal to " << ring->remainingCapacity() << std::endl;
        }if(thread == NULL) {
            startReader();
        }
        return true;
    }
    T take() {
        typename std::map<int, simpleRing<T>*>::iterator rit = this->buffer.begin();
        typename std::map<int, simpleRing<T>*>::iterator rit_end = this->buffer.end();
        if(rit == rit_end) {
            this->thread->interrupt();
            return "";
        }
        simpleRing<T>* ring = rit->second;

        if(ring->available() > 0) {
            std::cout << "Avalable: " << ring->available() << " from ring " << rit->first << std::endl;
            ring->take();
        }
        else if(ring->deleteRing()) {
            if(rit != rit_end && ring->deleteRing()) {
                std::cout << "Delete ring with " << rit->first << std::endl;
                while(!permitDel);
                this->permit = false;
                this->buffer.erase(rit);
                this->permit = true;
            }
        }
        return "";
    }
    void read() {
        std::cout << "Read thread starting5" << std::endl;
        while(this->thread->joinable()) {
            T element = this->take();
        }
    }
    void startReader() {
        std::cout << "Read thread starting" << std::endl;
        if(thread != NULL && thread->joinable()) {
            return;
        }
        this->thread = boost::shared_ptr<boost::thread>(new boost::thread(&RingBufferFlip4::read, this));
    }
private:
    boost::shared_ptr<boost::thread> thread;
    static const int max_Delay = 1;
    int capacity = 100;
    boost::atomic<unsigned int> writePos;
    boost::atomic<unsigned int> readPos;
    boost::atomic<bool> flipped;
    boost::atomic<bool> permit;
    boost::atomic<bool> permitDel;
    boost::atomic<unsigned int> number;
    boost::atomic<unsigned int> numFuture;
    boost::atomic<unsigned int> numEpoche;
    std::map<int, simpleRing<T>*> buffer;
};


#endif // NONBLOCKSTRUCTURE_H
