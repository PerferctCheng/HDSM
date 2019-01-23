#pragma once
#include "HDLinkedListNode.h"
#include "TypeDefine.h"

struct IHDLinkedList 
{
	virtual HDLinkedListNode get_node(HINT32 offset) = 0;
};