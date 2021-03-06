#ifndef _rbfm_h_
#define _rbfm_h_

#include <string>
#include <vector>
#include <climits>

#include "pfm.h"


static const uint16_t DELETED_MASK = 0x8000; // 1000 0000 ...
static const uint16_t TOMB_MASK = 0x4000;    // 0100 0000 ...

class Record;
struct Tombstone;

// Record ID
typedef struct {
    unsigned pageNum;    // page number
    unsigned slotNum;    // slot number in the page
} RID;

// Attribute
typedef enum {
    TypeInt = 0, TypeReal, TypeVarChar
} AttrType;

typedef unsigned AttrLength;

struct Attribute {
    std::string name;     // attribute name
    AttrType type;     // attribute type
    AttrLength length; // attribute length
};

// Comparison Operator (NOT needed for part 1 of the project)
typedef enum {
    EQ_OP = 0, // no condition// =
    LT_OP,      // <
    LE_OP,      // <=
    GT_OP,      // >
    GE_OP,      // >=
    NE_OP,      // !=
    NO_OP       // no condition
} CompOp;


/********************************************************************
* The scan iterator is NOT required to be implemented for Project 1 *
********************************************************************/

# define RBFM_EOF (-1)  // end of a scan operator

// RBFM_ScanIterator is an iterator to go through records
// The way to use it is like the following:
//  RBFM_ScanIterator rbfmScanIterator;
//  rbfm.open(..., rbfmScanIterator);
//  while (rbfmScanIterator(rid, data) != RBFM_EOF) {
//    process the data;
//  }
//  rbfmScanIterator.close();

class RBFM_ScanIterator {
public:
    RBFM_ScanIterator() = default;;

    ~RBFM_ScanIterator() = default;;

    // Never keep the results in the memory. When getNextRecord() is called,
    // a satisfying record needs to be fetched from the file.
    // "data" follows the same format as RecordBasedFileManager::insertRecord().
    void init(FileHandle &fileHandle, const std::vector<Attribute> &recordDescriptor,
            const std::string &conditionAttribute, const CompOp compOp, const void *value,
            const std::vector<std::string> &attributeNames);
    void moveToNextSlot(const uint16_t totalSlotNum);
    RC getNextRecord(RID &rid, void *data);
    RC close() { 
        delete[] conditionValue;
        return 0;
    };

    FileHandle* fileHandle;
private:
    unsigned currentPageNum;
    unsigned totalPageNum;
    uint16_t currentSlotNum;
    std::vector<Attribute> recordDescriptor;

    std::string conditionAttribute;
    char* conditionValue;
    AttrType conditionType;

    CompOp comOp;
    std::vector<std::string> selectedAttributeNames;
    bool finishScan;
};

class RecordBasedFileManager {
public:
    static RecordBasedFileManager &instance();                          // Access to the _rbf_manager instance

    RC createFile(const std::string &fileName);                         // Create a new record-based file

    RC destroyFile(const std::string &fileName);                        // Destroy a record-based file

    RC openFile(const std::string &fileName, FileHandle &fileHandle);   // Open a record-based file

    RC closeFile(FileHandle &fileHandle);                               // Close a record-based file

    //  Format of the data passed into the function is the following:
    //  [n byte-null-indicators for y fields] [actual value for the first field] [actual value for the second field] ...
    //  1) For y fields, there is n-byte-null-indicators in the beginning of each record.
    //     The value n can be calculated as: ceil(y / 8). (e.g., 5 fields => ceil(5 / 8) = 1. 12 fields => ceil(12 / 8) = 2.)
    //     Each bit represents whether each field value is null or not.
    //     If k-th bit from the left is set to 1, k-th field value is null. We do not include anything in the actual data part.
    //     If k-th bit from the left is set to 0, k-th field contains non-null values.
    //     If there are more than 8 fields, then you need to find the corresponding byte first,
    //     then find a corresponding bit inside that byte.
    //  2) Actual data is a concatenation of values of the attributes.
    //  3) For Int and Real: use 4 bytes to store the value;
    //     For Varchar: use 4 bytes to store the length of characters, then store the actual characters.
    //  !!! The same format is used for updateRecord(), the returned data of readRecord(), and readAttribute().
    // For example, refer to the Q8 of Project 1 wiki page.

    // Insert a record into a file
    RC insertRecord(FileHandle &fileHandle, const std::vector<Attribute> &recordDescriptor, const void *data, RID &rid);

    // Read a record identified by the given rid.
    RC readRecord(FileHandle &fileHandle, const std::vector<Attribute> &recordDescriptor, const RID &rid, void *data);

    // Print the record that is passed to this utility method.
    // This method will be mainly used for debugging/testing.
    // The format is as follows:
    // field1-name: field1-value  field2-name: field2-value ... \n
    // (e.g., age: 24  height: 6.1  salary: 9000
    //        age: NULL  height: 7.5  salary: 7500)
    RC printRecord(const std::vector<Attribute> &recordDescriptor, const void *data);

    /*****************************************************************************************************
    * IMPORTANT, PLEASE READ: All methods below this comment (other than the constructor and destructor) *
    * are NOT required to be implemented for Project 1                                                   *
    *****************************************************************************************************/
    // Delete a record identified by the given rid.
    RC deleteRecord(FileHandle &fileHandle, const std::vector<Attribute> &recordDescriptor, const RID &rid);

    // Assume the RID does not change after an update
    RC updateRecord(FileHandle &fileHandle, const std::vector<Attribute> &recordDescriptor, const void *data,
                    const RID &rid);
    //RC findRecord(FileHandle &fileHandle, );
    // Read an attribute given its name and the rid.
    RC readAttribute(FileHandle &fileHandle, const std::vector<Attribute> &recordDescriptor, const RID &rid,
                     const std::string &attributeName, void *data);

    // Scan returns an iterator to allow the caller to go through the results one by one.
    RC scan(FileHandle &fileHandle,
            const std::vector<Attribute> &recordDescriptor,
            const std::string &conditionAttribute,
            const CompOp compOp,                  // comparision type such as "<" and "="
            const void *value,                    // used in the comparison
            const std::vector<std::string> &attributeNames, // a list of projected attributes
            RBFM_ScanIterator &rbfm_ScanIterator);

    RC writeRecordFromTombstone(Record& record, FileHandle& fileHandle, const Tombstone& tombstone);
   
    uint32_t getNextAvailablePageNum(uint16_t insertSize, FileHandle& fileHandle, const uint32_t& pageFrom);
    uint32_t getPageNumWithEmptySlot(uint16_t insertSize, FileHandle& fileHandle);
    void appendPage(FileHandle &fileHandle);


    RC getAllIndex(FileHandle &fileHandle, uint32_t pageNum);
    RC printHex(FileHandle &fileHandle, uint32_t pageNum, uint16_t offset, uint16_t size);
    RC getAllVAR(FileHandle &fileHandle, uint32_t pageNum);
public:

protected:
    RecordBasedFileManager();                                                   // Prevent construction
    ~RecordBasedFileManager();                                                  // Prevent unwanted destruction
    RecordBasedFileManager(const RecordBasedFileManager &);                     // Prevent construction by copying
    RecordBasedFileManager &operator=(const RecordBasedFileManager &);          // Prevent assignment


private:
    static RecordBasedFileManager *_rbf_manager;
};


class Record {
public:
    Record(int none);
    Record(const Record&);
    Record(const std::vector<Attribute> &_descriptor, const void* _data, const RID &_rid);
    Record& operator=(const Record& r);
    ~Record();
    bool isNull(int fieldNum);
    static bool isMatch(AttrType type, const char* recordValue, const char* conditionValue, const CompOp comOp);
    void getAttribute(const std::string attrName, const std::vector<Attribute> &recordDescriptor, void* attr);
    uint32_t getAttributeSize(const std::string attrName, const std::vector<Attribute> &recordDescriptor);
    // get void* for write Page
    const uint8_t* getRecord() const;
    // convert Raw data to void*
    void decode(void* data) const;
    uint32_t getDataSize();

    // Record ID
    RID rid;
    // number of fields = descriptor.size();
    uint16_t numOfField; 
    // total rocord size in bytes (size + null part + index part + data part)
    uint16_t recordSize;
    uint16_t* indexData;


    // each index size used 2 byte: uint16_t:  65535
    const static uint16_t indexSize = 2;
    // record padding size = 1 byte;
    const static uint16_t paddingSize = 6;

private:
    void convertData(const std::vector<Attribute> &descriptor,const void* _data);
    // use Record::getRecord to access
    uint8_t* recordHead;

    // null indicator size (bytes) 
    const uint8_t* nullData;
    uint16_t indicatorSize;
    
};

enum dataPageVar {HEADER_OFFSET_FROM_END, RECORD_OFFSET_FROM_BEGIN, SLOT_NUM, RECORD_NUM};

class DataPage {
public:

    DataPage(const void* data);
    ~DataPage();

    void readRecord(FileHandle& fileHandle, uint16_t offset, uint16_t recordSize, void* data);
    void writeRecord(const Record &record, FileHandle &fileHandle, unsigned availablePage, RID &rid);

    void insertOutsideRecord(FileHandle& fileHandle, Record& record, uint32_t pageNum);
    void deleteRecordFromTombstone(FileHandle &fileHandle, uint32_t pageNum, Tombstone &tombstone, const int16_t tombstoneDiff);
    void shiftRecords(FileHandle& fileHandle, uint32_t pageNum, uint16_t startPos, int16_t diff);
    void shiftIndexes(FileHandle& fileHandle, uint32_t pagenum, uint16_t startPos, int16_t diff);

    void insertIndexPair(FileHandle &fileHandle, uint32_t pageNum, std::pair<uint16_t, uint16_t> indexPair);
    void updateRecord(FileHandle &fileHandle, const Record &record, uint32_t pagenum, uint16_t offset);
    void updateIndexPair(FileHandle& fileHandle, uint32_t pagenum, uint16_t slotNum, std::pair<uint16_t,uint16_t> newIndexPair);

    //  Update VAR
    void updateOffsetFromEnd(FileHandle &fileHandle, uint32_t pageNum, int16_t diff);
    void updateOffsetFromBegin(FileHandle &fileHandle, uint32_t pageNum, int16_t diff);
    void updateSlotNum(FileHandle &fileHandle, uint32_t pageNum, int16_t diff);
    void updateRecordNum(FileHandle &fileHandle, uint32_t pageNum, int16_t diff);

    // void insertTombstone(Tombstone &tombstone, FileHandle &fileHandle, const RID &rid, const uint16_t recordSize);
    void readTombstone(Tombstone &tombstone, const RID &rid);
    uint16_t findEmptySlot();
    void writeTombstone(FileHandle &fileHandle, uint32_t pageNum, Tombstone &tombstone, const uint16_t offsetFromBegin);
    unsigned getFreeSpaceSize();
    bool isRecord(FileHandle &fileHandle, const RID &rid);
    std::pair<uint16_t,uint16_t> getIndexPair(uint16_t index);

    //HEADER_OFFSET_FROM_END, RECORD_OFFSET_FROM_BEGIN, SLOT_NUM
    uint16_t var[DATA_PAGE_VAR_NUM];
    // std::pair<uint16_t,uint16_t>* pageHeader;   //  TODO: no use


    DataPage& operator=(const DataPage &dataPage);
    DataPage(const DataPage& d);

/////////////////////////////////////////////////////////////////////////////////////////////////
// TODO CHANGE BACK!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
// private:
    uint8_t* page;
/////////////////////////////////////////////////////////////////////////////////////////////////////
};

struct Tombstone {
    uint16_t flag;
    uint32_t pageNum;
    uint16_t slotNum;
};
#endif