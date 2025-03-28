#include <iostream>
#include <vector>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <random>
#include <chrono>
#include <iomanip>
#include <fstream>

class DungeonManager {
private:
    std::mutex mtx;
    std::condition_variable cv;

    int max_instances;
    int tank_queue;
    int healer_queue;
    int dps_queue;
    int min_time;
    int max_time;

    struct Instance {
        bool is_active = false;
        int parties_served = 0;
        std::chrono::duration<double> total_time = std::chrono::duration<double>::zero();
    };
    std::vector<Instance> instances;

    std::random_device rd;
    std::mt19937 gen;
    std::uniform_int_distribution<> time_dist;

    void runDungeon(int instance_id) {
        while (true) {
            std::unique_lock<std::mutex> lock(mtx);
    
            // Wait until we can form a party OR there's no possibility to do so
            cv.wait(lock, [this]() {
                return (tank_queue > 0 && healer_queue > 0 && dps_queue >= 3) || 
                       (tank_queue == 0 || healer_queue == 0 || dps_queue < 3);
            });
    
            // Recheck queue after waking up
            if (tank_queue == 0 || healer_queue == 0 || dps_queue < 3) {
                return; // No full parties can form, exit safely
            }
    
            // Assign players and mark instance as active
            instances[instance_id].is_active = true;
            tank_queue--;
            healer_queue--;
            dps_queue -= 3;
    
            lock.unlock(); // Release the lock before simulating dungeon run
    
            // Simulate dungeon run
            int run_time = time_dist(gen);
            std::this_thread::sleep_for(std::chrono::seconds(run_time));
    
            // Reacquire lock to update instance stats
            lock.lock();
            instances[instance_id].parties_served++;
            instances[instance_id].total_time += std::chrono::seconds(run_time);
            instances[instance_id].is_active = false;
    
            // Notify the next waiting instance safely
            cv.notify_one();
        }
    }
    
public:
    DungeonManager(int n, int t, int h, int d, int t1, int t2) 
        : max_instances(n), tank_queue(t), healer_queue(h), dps_queue(d), 
          min_time(t1), max_time(t2), instances(n), gen(rd()), 
          time_dist(t1, t2) {}

    void simulate() {
        std::vector<std::thread> instance_threads;
        std::thread status_thread(&DungeonManager::printLiveStatus, this);

        for (int i = 0; i < max_instances; ++i) {
            instance_threads.emplace_back(&DungeonManager::runDungeon, this, i);
        }

        for (auto& thread : instance_threads) {
            thread.join();
        }
        
        status_thread.join();
        printSummary();
    }

    void printLiveStatus() {
        while (true) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
            std::unique_lock<std::mutex> lock(mtx);

            bool allInstancesEmpty = true;
            for (const auto& instance : instances) {
                if (instance.is_active) {
                    allInstancesEmpty = false;
                    break;
                }
            }
            
            bool noMoreParties = (tank_queue == 0 || healer_queue == 0 || dps_queue < 3);
            if (noMoreParties && allInstancesEmpty) {
                return;
            }

            std::cout << "\n[Live Dungeon Status]\n";
            std::cout << std::string(40, '-') << std::endl;
            for (size_t i = 0; i < instances.size(); ++i) {
                std::cout << "Instance " << i + 1 << ": "
                          << (instances[i].is_active ? "Active" : "Empty") << std::endl;
            }
            std::cout << "Remaining in Queue - Tanks: " << tank_queue
                      << ", Healers: " << healer_queue
                      << ", DPS: " << dps_queue << std::endl;
        }
    }

    void printSummary() {
        std::cout << "\nDungeon Instance Summary:\n";
        std::cout << std::string(40, '-') << std::endl;
        
        for (int i = 0; i < max_instances; ++i) {
            std::cout << "Instance " << i + 1 << ": ";
            std::cout << "Status: " << (instances[i].is_active ? "Active" : "Empty") << std::endl;
            std::cout << "  Parties Served: " << instances[i].parties_served << std::endl;
            std::cout << "  Total Time Served: " << std::fixed << std::setprecision(2) 
                      << instances[i].total_time.count() << " seconds\n";
        }

        std::cout << "\nRemaining in Queue:\n";
        std::cout << "Tanks: " << tank_queue << std::endl;
        std::cout << "Healers: " << healer_queue << std::endl;
        std::cout << "DPS: " << dps_queue << std::endl;
    }
};

int main() {
    int n, t, h, d, t1, t2;

    try {
        std::ifstream inputFile("input.txt");
        if (!inputFile.is_open()) {
            throw std::runtime_error("Unable to open input file");
        }
        if (!(inputFile >> n >> t >> h >> d >> t1 >> t2)) {
            throw std::runtime_error("Error reading inputs from the file");
        }
        if (n <= 0 || t < 0 || h < 0 || d < 0 || t1 < 0 || t2 < 0 || t1 > t2 || t2 > 15) {
            throw std::runtime_error("Invalid input values");
        }

        DungeonManager manager(n, t, h, d, t1, t2);
        manager.simulate();
    }
    catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}