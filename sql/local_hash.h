#ifndef LOCAL_HASH
#define LOCAL_HASH

#include <string.h>


class ticket_duration_pair
{
public:
  ticket_duration_pair(MDL_ticket *_mdl_ticket, enum_mdl_duration _duration)
  {
    mdl_ticket= _mdl_ticket;
    duration= _duration;
  }
  MDL_ticket *mdl_ticket;
  enum_mdl_duration duration;
};

class key_duration_pair
{
public:
  key_duration_pair(MDL_key *_mdl_key, enum_mdl_duration _duration)
  {
    mdl_key= _mdl_key;
    duration= _duration;
  }
  MDL_key *mdl_key;
  enum_mdl_duration duration;
};




template <typename helper, typename comp_type> class local_hash
{
public:
  using T = typename helper::elem_type;

  MDL_key *get_key(T *elem) { return helper::get_key(elem); }

  local_hash()
  {
    // first.set_mark(true);
    capacity= START_CAPACITY;
    size= 0;
    hash_array= new T *[capacity] {};
  }

private:
  bool insert_helper_teml(MDL_key* mdl_key, T *value)
  {
    auto key= mdl_key->hash_value() % capacity;
    for (uint i= 1; i < capacity; i++)
    {
      if (hash_array[key] == nullptr)
      {
        hash_array[key]= value;
        size++;
        return true;
      }
      else if (hash_array[key] != value)
      {
        key = (key + 1) % capacity;
      }
      else
      {
        return false;
      }
    }

    return false;
  };
  
  bool is_equal(T *lhs, comp_type *rhs) { return helper::is_equal(lhs, rhs); }

  //bool is_equal(void* lhs, void* rhs) 
  //{ 
  //  return lhs == rhs;
  //}

  //bool is_equal(ticket_duration_pair *lhs, key_duration_pair *rhs) 
  //{
  //  //return false;
  //  return lhs->mdl_ticket->get_key()->is_equal(rhs->mdl_key) &&
  //         lhs->duration == rhs->duration;
  //}
  
public:
  T *find_teml(MDL_key *mdl_key, comp_type *value)
  {
    /* if (first.mark())
     {
       if (first.ptr() && !strcmp(first.ptr()->get_table_name(), table_name) &&
           !strcmp(first.ptr()->get_db_name(), db_name))
         return first.ptr();
       if (second != nullptr && !strcmp(second->get_table_name(), table_name)
     && !strcmp(second->get_db_name(), db_name)) return second;

       return nullptr;
     }*/

    auto key= mdl_key->hash_value() % capacity;

    for (uint i= 1; i < capacity; i++)
    {
      if (hash_array[key] != nullptr)
      {
        if (is_equal(hash_array[key], value))
          return hash_array[key];
        else
        {
          key = (key + 1) % capacity;
        }
      }
      else
      {
        return nullptr;
      }
    }

    return nullptr;
  };

private:
    void rehash_subsequence(uint i) 
    {
      for (int j= i + 1; hash_array[j] != nullptr; j= (j + 1) % capacity)
      {
        auto key= get_key(hash_array[j])->hash_value() % capacity;
        if (key <= j)
        {
          hash_array[i]= hash_array[j];
          i= j;
        }
      }

      hash_array[i]= nullptr;
    }

public:

  bool erase(MDL_key* mdl_key, comp_type* value) 
  { 
    auto key= mdl_key->hash_value() % capacity;

    for (uint i= 1; i < capacity; i++)
    {
      if (hash_array[key] != nullptr)
      {
        if (is_equal(hash_array[key], value))
        {
          hash_array[key]= nullptr;
          size--;
          rehash_subsequence(key);
          return true;
        }
        else
        {
          key = (key + 1) % capacity;
        }
      }
      else
      {
        return false;
      }
    }


   /* while (hash_array[key % capacity] != nullptr)
    {
      if (is_equal(hash_array[key % capacity], value) 
      {
        auto erase_key= key;
        key= (key + 1) % capacity;
        while (hash_array[key % capacity] != nullptr ||
               hash_array[key % capacity])
        {
          key= (key + 1) % capacity;          
        }

        if (hash_array[key % capacity] == nullptr) 
        {
          hash_array[erase_key % capacity]= nullptr;
        }
        else
        {
          
        }
      }
    
    }*/

    return false;
  }

  void rehash(uint _capacity)
  {
   /* uint past_capacity= capacity;
    capacity= _capacity;
    auto temp_hash_array= hash_array;
    hash_array= (value_type **) calloc(capacity, sizeof(value_type *));

    for (uint i= 0; i < past_capacity; i++)
    {
      if (temp_hash_array[i])
      {
        insert_helper(temp_hash_array[i]);
      }
    }

    delete[] temp_hash_array;*/
    return;
  }

  /*void init_hash_array()
  {
    TABLE_LIST *_first= first.ptr();
    TABLE_LIST *_second= second;

    capacity= START_CAPACITY;
    hash_array= (TABLE_LIST **) calloc(capacity, sizeof(TABLE_LIST *));
    size= 0;

    insert_helper(_first);
    insert_helper(_second);
  }*/

public:

  bool insert_teml(MDL_key *mdl_key, T *value)
  {
    /*if (first.mark())
    {
      if (!first.ptr())
      {
        first.set_ptr(tl);
        return true;
      }
      else if (!second)
      {
        second= tl;
        return true;
      }
      else
      {
        first.set_mark(false);
        init_hash_array();
      }
    }*/

    if (static_cast<double>(size + 1) >
        LOAD_FACTOR * static_cast<double>(capacity))
      rehash(2 * capacity);

    return insert_helper_teml(mdl_key, value);
  };

  
  bool clear()
  {
    /*if (!hash_array || first.mark())
      return false;*/

    if (!hash_array)
      return false;

    if (std::is_same<T, ticket_duration_pair>::value)
    {
      for (uint i= 0; i < capacity; i++)
      {
        hash_array[i]= nullptr;
      }
    }
    else
    {
      for (uint i= 0; i < capacity; i++)
      {
        hash_array[i]= nullptr;
      }
    }

    capacity= START_CAPACITY;
    return true;
  }

private:
  static constexpr uint START_CAPACITY= 256;
  static constexpr double LOAD_FACTOR= 0.5f;


  class markable_reference
  {
  public:
    void set_ptr(T *tl) { p= reinterpret_cast<uintptr_t>(tl); }
    T *ptr() { return reinterpret_cast<T *>(p); }

    void set_mark(bool mark) { low_type= mark; }
    bool mark() { return low_type; };

  private:
    bool low_type : 1;
    uintptr_t p : 63;
  };

  union
  {
    struct
    {
      markable_reference first;
      T *second;
    };
    struct
    {
      T **hash_array;
      uint32 size;
      uint32 capacity;
    };
  };
};

#endif
