#ifdef USE_PRAGMA_IMPLEMENTATION
#pragma implementation				// gcc: Class implementation
#endif

#include "mariadb.h"
#include "sql_priv.h"
#include <m_ctype.h>
#include "sql_select.h"

#include "opt_trace.h"

/*
  @brief
    Check if passed item is "UCASE(table.key_part_col)"

  @return 
     Argument of the UCASE if passed item matches
     NULL otherwise.
*/
static Item* is_upper_key_col(Item *item)
{
  if (item->type() == Item::FUNC_ITEM)
  {
    Item_func *item_func= (Item_func*)item;
    if (item_func->functype() == Item_func::UCASE_FUNC)
    {
      Item *arg= item_func->arguments()[0];
      Item *arg_real= arg->real_item();
      if (arg_real->type() == Item::FIELD_ITEM)
      {
        Item_field *item_field= (Item_field*)arg_real;
        enum_field_types field_type= item_field->field_type();
        if ((field_type == MYSQL_TYPE_VAR_STRING ||
             field_type == MYSQL_TYPE_VARCHAR) &&
            item_field->field->flags & PART_KEY_FLAG)
        {
          return arg;
        }
      }
    }
  }
  return nullptr;
}


static void trace_upper_removal_rewrite(THD *thd, Item *old_item, Item *new_item)
{
  Json_writer_object trace_wrapper(thd);
  Json_writer_object obj(thd, "sargable_casefold_removal");
  obj.add("before", old_item)
     .add("after", new_item);
}


/*
  @brief
    Rewrite UPPER(key_varchar_col) = expr into key_varchar_col=expr

  @detail
    UPPER() may occur on both sides of the equality.
    UCASE() is a synonym of UPPER() so we handle it, too.
*/

Item* Item_func_eq::varchar_upper_cmp_transformer(THD *thd, uchar *arg)
{
  if (cmp.compare_type() == STRING_RESULT &&
      cmp.compare_collation()->state & MY_CS_UPPER_EQUAL_AS_EQUAL)
  {
    Item *arg0= arguments()[0];
    Item *arg1= arguments()[1];
    bool do_rewrite= false;
    Item *tmp;
    
    // Try rewriting the left argument
    if ((tmp= is_upper_key_col(arguments()[0])))
    {
      arg0= tmp;
      do_rewrite= true;
    }

    // Try rewriting the right argument
    if ((tmp= is_upper_key_col(arguments()[1])))
    {
      arg1=tmp;
      do_rewrite= true;
    }

    if (do_rewrite)
    {
      Item *res= new (thd->mem_root) Item_func_eq(thd, arg0, arg1);
      if (res && !res->fix_fields(thd, &res))
      {
        trace_upper_removal_rewrite(thd, this, res);
        return res;
      }
    }
  }
  return this;
}


/*
  @brief
    Rewrite "UPPER(key_col) IN (const-list)" into "key_col IN (const-list)"
*/

Item* Item_func_in::varchar_upper_cmp_transformer(THD *thd, uchar *arg)
{
  if (arg_types_compatible &&
      m_comparator.cmp_type() == STRING_RESULT &&
      cmp_collation.collation->state & MY_CS_UPPER_EQUAL_AS_EQUAL &&
      all_items_are_consts(args + 1, arg_count - 1))
  {
    Item *arg0= arguments()[0];
    Item *tmp;
    if ((tmp= is_upper_key_col(arg0)))
    {
      Item_func_in *cl= (Item_func_in*)build_clone(thd);
      Item *res;
      cl->arguments()[0]= tmp;
      cl->walk(&Item::cleanup_excluding_const_fields_processor, 0, 0);
      res= cl;
      if (res->fix_fields(thd, &res))
        return this;
      trace_upper_removal_rewrite(thd, this, res);
      return res;
    }
  }
  return this;
}

