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

template <typename value_type, typename comp_type> class local_hash
{
public:
  local_hash()
  {
    // first.set_mark(true);
    capacity= START_CAPACITY;
    size= 0;
    hash_array= new value_type *[capacity] {};
  }

private:
  bool insert_helper_teml(MDL_key* mdl_key, value_type *value)
  {
    auto key= mdl_key->hash_value();
    for (uint i= 1; i < capacity; i++)
    {
      if (hash_array[key % capacity] == nullptr)
      {
        hash_array[key % capacity]= value;
        size++;
        return true;
      }
      else if (hash_array[key % capacity] != value)
      {
        key+= i;
      }
      else
      {
        return false;
      }
    }

    return false;
  };
  

  bool is_equal(void* lhs, void* rhs) 
  { 
    return lhs == rhs;
  }

  bool is_equal(ticket_duration_pair *lhs, key_duration_pair *rhs) 
  {
    return lhs->mdl_ticket->get_key()->is_equal(rhs->mdl_key) &&
           lhs->duration == rhs->duration;
  }
  
public:
  value_type *find_teml(MDL_key *mdl_key, comp_type *value)
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

    auto key= mdl_key->hash_value();

    for (uint i= 1; i < capacity; i++)
    {
      if (hash_array[key % capacity] != nullptr)
      {
        auto tt= hash_array[key % capacity];
        /*if (hash_array[key % capacity] == value)
          return hash_array[key % capacity];*/
        if (is_equal(hash_array[key % capacity], value))
          return hash_array[key % capacity];
        else
        {
          key+= i;
        }
      }
      else
      {
        return nullptr;
      }
    }

    return nullptr;
  };

  bool erase(MDL_key* mdl_key, comp_type* value) 
  { 
    auto el= find_teml(mdl_key, value);
    el= nullptr;

    return true;
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

  bool insert_teml(MDL_key *mdl_key, value_type *value)
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

    if (std::is_same<value_type, ticket_duration_pair>::value)
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
    void set_ptr(value_type *tl) { p= reinterpret_cast<uintptr_t>(tl); }
    value_type *ptr() { return reinterpret_cast<value_type *>(p); }

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
      value_type *second;
    };
    struct
    {
      value_type **hash_array;
      uint32 size;
      uint32 capacity;
    };
  };
};

#endif
