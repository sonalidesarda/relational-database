/**
 * @file heap_storage_test.cpp
 * @author Luan (Remi) Ta, Helen Huang
 * @brief tests for the heresy that is pointer arithmetic, bc otherwise I might die debugging
 * @version 0.1
 * @date 2022-04-13
 */

#include "db_cxx.h"
#include "heap_storage.h"
#include <string.h>
#include <memory>

DbEnv *_DB_ENV;

bool test_heap_storage();

int main(){

    _DB_ENV = new DbEnv(0U);
    _DB_ENV->set_message_stream(&std::cout);
	_DB_ENV->set_error_stream(&std::cerr);
	_DB_ENV->open("./", DB_CREATE | DB_INIT_MPOOL, 0);

    std::cout << std::endl;
    std::cout << "\nCreating/Opening HeapFile..." << std::endl;
    HeapFile hFile("TestRelation");
    
    try {
        hFile.create();
        std::cout << "TestRelation.poo created" << std::endl; 
    } catch(...) {
        hFile.open();
        std::cout << "TestRelation.poo opened" << std::endl; 
    }

    char* testStr1 = (char*) "Here, cometh the age of inquiry, not of nature but Nature Within.";
    char* testStr2 = (char*) "Lord of the pillared halls. What reign do you have over us? What dominance do you hold over the sovereignty of Creation?";
    char* testStr3 = (char*) "As the path stretches into darkness, into fear, doubt, and loneliness.";
    char* testStr4 = (char*) "Beyond the Circles of the World, I will not pursue them, for beyond the Circles of the World there is Nothing. But within them they shall not escape me, until they enter into Nothing.";
    char* testStr5 = (char*) "Last light of the Alder days... when all of creation was raw and violent... when that most fundamental nature of things waged war within the crust of existence. And what else is there to war over but existence itself? Indeed, it is how they all came to be. Fragments of nothingness fought against the Primal Will - unbinding themselves from the chains of timelessness to instill, within their lack of matters, a dim, fabled, light of life. For it is within this light that the world began. Amidst chaos, the renegades of absolute silence rose and incurred the undying wrath of God, infinite in all but life and existence, which are now stolen from Him.";
    char* testStr6 = (char*) "I seek the Lamb, so that I may graze the Divine grassland.";
    char* testStr7 = (char*) "Ista, before the days. Whole.";
    
    Dbt testBlk1(testStr1, strlen(testStr1) + 1);
    Dbt testBlk2(testStr2, strlen(testStr2) + 1);
    Dbt testBlk3(testStr3, strlen(testStr3) + 1);
    Dbt testBlk4(testStr4, strlen(testStr4) + 1);
    Dbt testBlk5(testStr5, strlen(testStr5) + 1);
    Dbt testBlk6(testStr6, strlen(testStr6) + 1);
    Dbt testBlk7(testStr7, strlen(testStr7) + 1);

    std::cout << "\nAcquiring Slottedpage->.." << std::endl;
    std::unique_ptr<SlottedPage> page(hFile.get_new());
    
    u_int16_t id;
    std::cout << "Adding str1: " << std::endl << testStr1 << std::endl;
    id = page->add(&testBlk1);
    std::cout << "ID: " << id << std::endl;
    std::cout << "Content: " << std::unique_ptr<char>((char*)std::unique_ptr<Dbt>(page->get(id))->get_data()).get() << std::endl;
    std::cout << std::endl;

    std::cout << "Adding str2: " << std::endl << testStr2 << std::endl;
    id = page->add(&testBlk2);
    std::cout << "ID: " << id << std::endl;
    std::cout << "Content: " << std::unique_ptr<char>((char*)std::unique_ptr<Dbt>(page->get(id))->get_data()).get() << std::endl;    
    std::cout << std::endl;

    std::cout << "Adding str3: " << std::endl << testStr3 << std::endl;
    id = page->add(&testBlk3);
    std::cout << "ID: " << id << std::endl;
    std::cout << "Content: " << std::unique_ptr<char>((char*)std::unique_ptr<Dbt>(page->get(id))->get_data()).get() << std::endl;    
    std::cout << std::endl;

    std::cout << "Adding str4: " << std::endl << testStr4 << std::endl;
    id = page->add(&testBlk4);
    std::cout << "ID: " << id << std::endl;
    std::cout << "Content: " << std::unique_ptr<char>((char*)std::unique_ptr<Dbt>(page->get(id))->get_data()).get() << std::endl;    
    std::cout << std::endl;
    
    std::cout << "Adding str5: " << std::endl << testStr5 << std::endl;
    id = page->add(&testBlk5);
    std::cout << "ID: " << id << std::endl;
    std::cout << "Content: " << std::unique_ptr<char>((char*)std::unique_ptr<Dbt>(page->get(id))->get_data()).get() << std::endl;    
    std::cout << std::endl;
    
    std::cout << "Adding str6: " << std::endl << testStr6 << std::endl;
    id = page->add(&testBlk6);
    std::cout << "ID: " << id << std::endl;
    std::cout << "Content: " << std::unique_ptr<char>((char*)std::unique_ptr<Dbt>(page->get(id))->get_data()).get() << std::endl;    
    std::cout << std::endl;
    
    std::cout << "Adding str7: " << std::endl << testStr7 << std::endl;
    id = page->add(&testBlk7);
    std::cout << "ID: " << id << std::endl;
    std::cout << "Content: " << std::unique_ptr<char>((char*)std::unique_ptr<Dbt>(page->get(id))->get_data()).get() << std::endl;
    std::cout << std::endl;
    std::cout << "Deleting odds..." << std::endl;

    int max = 0;
    std::unique_ptr<RecordIDs> curIds(page->ids());
    for (RecordID &i : *curIds) {
        if (i % 2 != 0) page->del(i);
        try {
            std::unique_ptr<Dbt> data (page->get(i));
            std::cout << "Record " << i << " exists with content " << std::unique_ptr<char>((char*)data->get_data()).get() << std::endl;
            if (max < i) max = i;
        } catch (...) {
            std::cout << "Record " << i << " NOT FOUND! " << std::endl;
        }
    }
    std::cout << std::endl;
    
    std::cout << "Testing put() on all records (even deleted records)..." << std::endl;
    for (RecordID i = 1; i <= max; i++) {
        char tmp[] = {'A','B','C','D', i + '0', '\0'};
        Dbt blk(tmp, strlen(tmp) + 1);
        page->put(i, blk);

        std::cout << "ID: " << i << std::endl;
        std::cout << "Content: " << std::unique_ptr<char>((char*)std::unique_ptr<Dbt>(page->get(i))->get_data()).get() << std::endl;
    }

    if (test_heap_storage()){
        std::cout << "\nProf. Lundeen's tests passed." << std::endl;
    }else {
        std::cout << "\nProf. Lundeen's tests FAILED!!!!" << std::endl;
    }

    hFile.drop();

    delete _DB_ENV;
    return 0;
}

// bool test_heap_storage() {

// 	ColumnNames column_names;
// 	column_names.push_back("a");
// 	column_names.push_back("b");
// 	ColumnAttributes column_attributes;
// 	ColumnAttribute ca(ColumnAttribute::INT);
// 	column_attributes.push_back(ca);
// 	ca.set_data_type(ColumnAttribute::TEXT);
// 	column_attributes.push_back(ca);
//     HeapTable table1("_test_create_drop_cpp", column_names, column_attributes);

//     table1.create();
//     std::cout << "\ncreate ok" << std::endl;
//     table1.drop();  // drop makes the object unusable because of BerkeleyDB restriction -- maybe want to fix this some day
//     std::cout << "\ndrop ok" << std::endl;

//     HeapTable table("_test_data_cpp", column_names, column_attributes);
//     table.create_if_not_exists();
//     std::cout << "\ncreate_if_not_exsts ok" << std::endl;

//     ValueDict row;
//     row["a"] = Value(12);
//     row["b"] = Value("Hello!");
//     std::cout << "\ntry insert" << std::endl;
//     table.insert(&row);
//     std::cout << "\ninsert ok" << std::endl;
//     std::unique_ptr<Handles> handles(table.select());
//     std::cout << "\nselect ok " << handles->size() << std::endl;
//     std::unique_ptr<ValueDict> result (table.project((*handles)[0]));
//     std::cout << "\nproject ok" << std::endl;

//     Value value = (*result)["a"];
//     if (value.n != 12){
//         table.drop();
//     	return false;
//     }
    
//     value = (*result)["b"];
//     if (value.s != "Hello!"){
//         table.drop();
// 		return false;
//     }
//     table.drop();

//     return true;
// }