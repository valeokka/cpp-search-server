#include <algorithm>
#include <cstdlib>
#include <future>
#include <map>
#include <numeric>
#include <type_traits>
#include <vector>
#include <mutex>

using namespace std::string_literals;

template <typename Key, typename Value>
class ConcurrentMap {
public:
    static_assert(std::is_integral_v<Key>, "ConcurrentMap supports only integer keys"s);

    struct Bucket{
        std::map<Key, Value> data;
        std::mutex mutex_value;
    };

    struct Access {
        Access (Bucket& bucket, const Key &key) :
        m_guard(bucket.mutex_value), ref_to_value(bucket.data[key])
        {}

        std::lock_guard<std::mutex> m_guard;
        Value& ref_to_value;
    };
    
    explicit ConcurrentMap(size_t bucket_count):
    buckets_(bucket_count)
    {}

    Access operator[](const Key &key){
        std::uint64_t uKey = static_cast<uint64_t>(key);
	    std::uint64_t key_num = uKey % buckets_.size();
        return {buckets_[key_num], key};
    }
    
    void Delete(const Key& key){
        std::uint64_t uKey = static_cast<uint64_t>(key);
	    std::uint64_t key_num = uKey % buckets_.size();
    
        std::lock_guard<std::mutex> m_guard(buckets_[key_num].mutex_value);
        buckets_[key_num].data.erase(key);
    }

   std::map<Key, Value> BuildOrdinaryMap(){
        std::map<Key, Value> result;
        for (size_t i = 0; i < buckets_.size(); ++i){
            for(const auto& [key, val] : buckets_[i].data){
            result[key] =Access(buckets_[i], key).ref_to_value;
            }
        }
        return result;
    }

private:
    std::vector<Bucket> buckets_;

};
