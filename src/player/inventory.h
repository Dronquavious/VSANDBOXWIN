#ifndef INVENTORY_H
#define INVENTORY_H

struct InventoryItem {
    int blockID; // 0 = empty
    int count;   
};

struct Inventory {
    InventoryItem slots[36]; // 9 hotbar, 27 storage
    int selectedSlot;        // 0-8
};

#endif