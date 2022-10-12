// Copyright 2020 The Khronos Group
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <vector>
#include <memory>

template<typename ValueType, typename ContainerType = std::vector<ValueType>>
struct OptionalList
{
  void ensureList()
  {
    if(!list)
      list = std::make_unique<ContainerType>();
  }

  void clear()
  {
    if(list)
      list->resize(0);
  }

  void push_back(const ValueType& value)
  {
    ensureList();
    list->push_back(value);
  }

  void emplace_back(const ValueType& value)
  {
    ensureList();
    list->emplace_back(value);
  }

  const ValueType* data() 
  { 
    if(list) 
      return list->data();
    return nullptr;
  }

  size_t size()
  {
    if(list) 
      return list->size();
    return 0;
  }

  std::unique_ptr<ContainerType> list; 
};