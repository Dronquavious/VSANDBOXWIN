#ifndef INVENTORY_H
#define INVENTORY_H

/**
 * represents a single slot in the inventory
 */
struct InventoryItem {
    int blockID; // int for serialization, cast to BlockType
    int count;   
};

/**
 * holds player inventory data
 * contains hotbar and storage slots
 */
struct Inventory {
    InventoryItem slots[36]; // 0-8 hotbar, 9-35 storage
    int selectedSlot;        // current hotbar index (0-8)
};

#endif