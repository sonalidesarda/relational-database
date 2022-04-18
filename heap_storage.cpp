#include "heap_storage.h"
#include <memory>
bool test_heap_storage() {return true;}

// 2 Bytes
typedef u_int16_t u16;

/* Constructor
end_free: ptr to end of free space
*/
SlottedPage::SlottedPage(Dbt &block, BlockID block_id, bool is_new) : DbBlock(block, block_id, is_new) {
    if (is_new) {
        this->num_records = 0;
        this->end_free = DbBlock::BLOCK_SZ - 1;
        put_header();
    } else {
        get_header(this->num_records, this->end_free);
    }
}

// Add a new record to the block. Return its id.
RecordID SlottedPage::add(const Dbt* data) {
    if (!has_room(data->get_size()))
        throw DbBlockNoRoomError("not enough room for new record");
    u16 id = ++this->num_records;
    u16 size = (u16) data->get_size();
    this->end_free -= size;
    u16 loc = this->end_free + 1;
    put_header();
    put_header(id, size, loc);
    memcpy(this->address(loc), data->get_data(), size);
    return id;
}

// Get a record's data for a given record id
// Dbt = a wrapper of berkeley database, points to a buffer (loc, size)
Dbt* SlottedPage::get(RecordID record_id){
    u16 size;
    u16 loc;
    get_header(size, loc, record_id);
    if (loc == 0){
        throw "Record not found";
    }

    // ptr of char = char array in C
    char* record_data = new char [size];
    // copy data from this array to another array
    // copy starting from location for the number of bytes = size
    memcpy(record_data,this->address(loc),size);

    Dbt* data = new Dbt(record_data,size);
    return data;
}

// Get the size and offset for given id. For id of zero, it is the block header.
void SlottedPage::get_header(u16 &size, u16 &loc, RecordID id){
    size = get_n(4 * id);
    loc = get_n(4 * id + 2);
}

// Replace the record with the given data.
// Raises ValueError if it won't fit.
void SlottedPage::put(RecordID record_id, const Dbt &data){
    u16 size;
    u16 loc;
    u16 new_size;
    u16 extra;

    get_header(size,loc,record_id);
    new_size = data.get_size();

    if(new_size > size && !has_room(new_size-size)){
        fprintf(stderr, "Not enough room in block \n"); 
    } 
    
    // delete the old record, the block records will be 
    // adjusted when delete the old record
    if(size != 0){
        del(record_id);
    }

    // add the data
    size = (u16) data.get_size();
    // move end free left to add in new record at end_free + 1
    this->end_free -= size;
    loc = this->end_free + 1;
    // update record 0 when removed a record
    // record 0 hold number of records, and end free
    put_header();
    put_header(record_id,size,loc);
    memcpy(this->address(loc), data.get_data(),size);
}

/*
    If start < end, then remove data from offset start up to but not including offset end by sliding data
            that is to the left of start to the right. If start > end, then make room for extra data from end to start
            by sliding data that is to the left of start to the left.
            Also fix up any record headers whose data has slid. Assumes there is enough room if it is a left
            shift (end < start).
*/
void SlottedPage::slide(u16 start, u16 end){
    u16 shift = end - start;
    u16 sizeOfChunk = start - end_free -1;
    
    if(shift == 0){
        return;
    }

    memcpy(this->address(end_free + 1 + shift),this->address(end_free+1),sizeOfChunk);

    
    // fixup headers (allowing change in ids using &)
        for (RecordID &id : *this->ids()){
            u16 size;
            u16 loc; 
            get_header(size,loc,id);
            if (loc <= start){
                loc += shift;
                put_header(id, size, loc);
            } 
        }
            
    end_free += shift;
    put_header();
}

// Calculate if we have room to store a record with given size.
// The size should include the 4 bytes for the header, too, if this is an add.
bool SlottedPage::has_room(u16 size){
    u16 available;
    available = end_free - (num_records + 1) * 4;
    return size <= available;
}

void SlottedPage::del(RecordID record_id){
    u16 size;
    u16 loc;
    get_header(size,loc,record_id);
    //delete record
    put_header(record_id,0,0);
    slide(loc,loc+size);  
    
}

RecordIDs * SlottedPage::ids(void){
    u16 size;
    u16 loc;
    RecordIDs* ids = new RecordIDs();
    for(u16 i = 1; i < num_records + 1; i++){
        get_header(size,loc,i);
        if(loc != 0){
            ids->push_back(i);
        }
    }
    return ids;
}

// Get 2-byte integer at given offset in block.
u16 SlottedPage::get_n(u16 offset) {
    return *(u16*)this->address(offset);
}

// Put a 2-byte integer at given offset in block.
void SlottedPage::put_n(u16 offset, u16 n) {
    *(u16*)this->address(offset) = n;
}

// Make a void* pointer for a given offset into the data block.
void* SlottedPage::address(u16 offset) {
    return (void*)((char*)this->block.get_data() + offset);
}

// Store the size and offset for given id. For id of zero, store the block header.
void SlottedPage::put_header(RecordID id, u16 size, u16 loc) {
    // first block in the slotted page which hold the number of records and the end_free
    // update the number of records and end_free if id == 0, this is record id
    if (id == 0) { // called the put_header() version and using the default params
        size = this->num_records;
        loc = this->end_free;
    }
    put_n(4*id, size);
    put_n(4*id + 2, loc);
}

// return the bits to go into the file
// caller responsible for freeing the returned Dbt and its enclosed ret->get_data().
// Dbt* HeapTable::marshal(const ValueDict* row) {
//     char *bytes = new char[DbBlock::BLOCK_SZ]; // more than we need (we insist that one row fits into DbBlock::BLOCK_SZ)
//     uint offset = 0;
//     uint col_num = 0;
//     for (auto const& column_name: this->column_names) {
//         ColumnAttribute ca = this->column_attributes[col_num++];
//         ValueDict::const_iterator column = row->find(column_name);
//         Value value = column->second;
//         if (ca.get_data_type() == ColumnAttribute::DataType::INT) {
//             *(int32_t*) (bytes + offset) = value.n;
//             offset += sizeof(int32_t);
//         } else if (ca.get_data_type() == ColumnAttribute::DataType::TEXT) {
//             uint size = value.s.length();
//             *(u16*) (bytes + offset) = size;
//             offset += sizeof(u16);
//             memcpy(bytes+offset, value.s.c_str(), size); // assume ascii for now
//             offset += size;
//         } else {
//             throw DbRelationError("Only know how to marshal INT and TEXT");
//         }
//     }
//     char *right_size_bytes = new char[offset];
//     memcpy(right_size_bytes, bytes, offset);
//     delete[] bytes;
//     Dbt *data = new Dbt(right_size_bytes, offset);
//     return data;
// }

// Handles* HeapTable::select(const ValueDict* where) {
//     Handles* handles = new Handles();
//     BlockIDs* block_ids = file.block_ids();
//     for (auto const& block_id: *block_ids) {
//         SlottedPage* block = file.get(block_id);
//         RecordIDs* record_ids = block->ids();
//         for (auto const& record_id: *record_ids)
//             handles->push_back(Handle(block_id, record_id));
//         delete record_ids;
//         delete block;
//     }
//     delete block_ids;
//     return handles;
// }