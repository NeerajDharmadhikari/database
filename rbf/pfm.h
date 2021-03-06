#ifndef _pfm_h_
#define _pfm_h_

typedef unsigned PageNum;
typedef int RC;
typedef unsigned char byte;

#define PAGE_SIZE 4096

#include <string>
#include <cstring>
#include <climits>
#include <fstream>
#include <vector>
#include <utility>


class FileHandle;
class HiddenPage;

#define HIDDEN_PAGE_VAR_NUM 7
#define NON_BTREE_VAR_NUM 4
#define DATA_PAGE_VAR_NUM 4

class PagedFileManager {
public:
    static PagedFileManager &instance();                                // Access to the _pf_manager instance

    RC createFile(const std::string &fileName);                         // Create a new file
    RC destroyFile(const std::string &fileName);                        // Destroy a file
    RC openFile(const std::string &fileName, FileHandle &fileHandle);   // Open a file
    RC closeFile(FileHandle &fileHandle);                               // Close a file

protected:
    PagedFileManager();                                                 // Prevent construction
    ~PagedFileManager();                                                // Prevent unwanted destruction
    PagedFileManager(const PagedFileManager &);                         // Prevent construction by copying
    PagedFileManager &operator=(const PagedFileManager &);              // Prevent assignment

private:
    static PagedFileManager *_pf_manager;
};

class FileHandle {
public:
    // variables to keep the counter for each operation
    unsigned readPageCounter;
    unsigned writePageCounter;
    unsigned appendPageCounter;
    static const int PAGE_OFFSET = 1;

    FileHandle();                                                       // Default constructor
    ~FileHandle();                                                      // Destructor

    RC readPage(PageNum pageNum, void *data);     // TODO: read datapage                      // Get a specific page
    RC writePage(PageNum pageNum, const void *data);                    // Write a specific page
    RC appendPage(const void *data);                                    // Append a specific page

    RC openFile(const std::string &fileName);   // TODO: read hiddenpage
    RC closeFile();                             // TODO: wirte hiddenpage
    RC isOpen();
    unsigned getNumberOfPages();                                        // Get the number of pages in the file
    RC collectCounterValues(unsigned &readPageCount, unsigned &writePageCount,
                            unsigned &appendPageCount);                 // Put current counter values into variables

    RC readBTreeHiddenPage(void *data);
    RC writeBTreeHiddenPage(void *data);
    // RC appendBTreePage(void *data);
    RC readBTreePage(PageNum pageNum, void *data);
    RC writeBTreePage(PageNum pageNum, const void *data);
    RC createNodePage(void *data);
    unsigned getNumberOfNodes();
private:
    
    //bool checkPageNum(int);

    std::fstream file;
    HiddenPage* hiddenPage;
    //DataPage* currentpage;
};

enum counter {READ_PAGE_COUNTER, WRITE_PAGE_COUNTER, APPEND_PAGE_COUNTER, PAGE_NUM
        , ROOT_PAGE_NUM, TOTAL_PAGE_NUM, ATTRTYPE};

class HiddenPage {
public:
    unsigned var[HIDDEN_PAGE_VAR_NUM];

    //unsigned size;

    // unsigned pageNum;


    HiddenPage();
    ~HiddenPage();

    void readHiddenPage(std::fstream& file);
    void writeHiddenPage(std::fstream& file);

private:
    //HiddenPage * hiddenPage;
};
#endif