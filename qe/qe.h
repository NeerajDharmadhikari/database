#ifndef _qe_h_
#define _qe_h_

#include "../rbf/rbfm.h"
#include "../rm/rm.h"
#include "../ix/ix.h"

#include <iostream>
#include <vector>
#include <map>

#define QE_EOF (-1)  // end of the index scan

typedef enum {
    MIN = 0, MAX, COUNT, SUM, AVG
} AggregateOp;

// The following functions use the following
// format for the passed data.
//    For INT and REAL: use 4 bytes
//    For VARCHAR: use 4 bytes for the length followed by the characters

struct Value {
    AttrType type;          // type of value
    void *data;             // value
};

struct Condition {
    std::string lhsAttr;        // left-hand side attribute
    CompOp op;                  // comparison operator
    bool bRhsIsAttr;            // TRUE if right-hand side is an attribute and not a value; FALSE, otherwise.
    std::string rhsAttr;        // right-hand side attribute if bRhsIsAttr = TRUE
    Value rhsValue;             // right-hand side value if bRhsIsAttr = FALSE

//    Condition();
//    ~Condition();
//    Condition operator= (const Condition &rCondition);
};

class Iterator {
    // All the relational operators and access methods are iterators.
public:
    virtual RC getNextTuple(void *data) = 0;

    virtual void getAttributes(std::vector<Attribute> &attrs) const = 0;

    virtual ~Iterator() = default;

    static RC mergeTwoTuple(const std::vector<Attribute> &leftAttribute, const char *leftTuple, const std::vector<Attribute> &rightAttrbute, const char *rightTuple, void *mergedTuple);
    static RC searchAttribute(const std::vector<Attribute> &attrs, const std::string &attrName, Attribute &attr);
};

class TableScan : public Iterator {
    // A wrapper inheriting Iterator over RM_ScanIterator
public:
    RelationManager &rm;
    RM_ScanIterator *iter;
    std::string tableName;
    std::vector<Attribute> attrs;
    std::vector<std::string> attrNames;
    RID rid{};

    TableScan(RelationManager &rm, const std::string &tableName, const char *alias = NULL) : rm(rm) {
        //Set members
        this->tableName = tableName;

        // Get Attributes from RM
        rm.getAttributes(tableName, attrs);

        // Get Attribute Names from RM
        for (Attribute &attr : attrs) {
            // convert to char *
            attrNames.push_back(attr.name);
        }

        // Call RM scan to get an iterator
        iter = new RM_ScanIterator();
        rm.scan(tableName, "", NO_OP, NULL, attrNames, *iter);

        // Set alias
        if (alias) this->tableName = alias;
    };

    // Start a new iterator given the new compOp and value
    void setIterator() {
        iter->close();
        delete iter;
        iter = new RM_ScanIterator();
        rm.scan(tableName, "", NO_OP, NULL, attrNames, *iter);
    };

    RC getNextTuple(void *data) override {
        return iter->getNextTuple(rid, data);
    };

    void getAttributes(std::vector<Attribute> &attributes) const override {
        attributes.clear();
        attributes = this->attrs;

        // For attribute in std::vector<Attribute>, name it as rel.attr
        for (Attribute &attribute : attributes) {
            std::string tmp = tableName;
            tmp += ".";
            tmp += attribute.name;
            attribute.name = tmp;
        }
    };

    ~TableScan() override {
        iter->close();
    };
};

class IndexScan : public Iterator {
    // A wrapper inheriting Iterator over IX_IndexScan
public:
    RelationManager &rm;
    RM_IndexScanIterator *iter;
    std::string tableName;
    std::string attrName;
    std::vector<Attribute> attrs;
    char key[PAGE_SIZE]{};
    RID rid{};

    IndexScan(RelationManager &rm, const std::string &tableName, const std::string &attrName, const char *alias = NULL)
            : rm(rm) {
        // Set members
        this->tableName = tableName;
        this->attrName = attrName;


        // Get Attributes from RM
        rm.getAttributes(tableName, attrs);

        // Call rm indexScan to get iterator
        iter = new RM_IndexScanIterator();
        if(rm.indexScan(tableName, attrName, NULL, NULL, true, true, *iter)!= 0) {
            // std::cerr << "SCAN DAMN FUCK" <<std::endl;
        }

        // Set alias
        if (alias) this->tableName = alias;
    };

    // Start a new iterator given the new key range
    void setIterator(void *lowKey, void *highKey, bool lowKeyInclusive, bool highKeyInclusive) {
        iter->close();
        delete iter;
        iter = new RM_IndexScanIterator();
        rm.indexScan(tableName, attrName, lowKey, highKey, lowKeyInclusive, highKeyInclusive, *iter);
    };

    RC getNextTuple(void *data) override {
        int rc = iter->getNextEntry(rid, key);
        // std::cerr << "IndexScan:getNextTuple: " << ((int*)key)[0] << " " << rc << 
        // std::endl;
        if (rc == 0) {
            rc = rm.readTuple(tableName, rid, data);
        }
        return rc;
    };

    void getAttributes(std::vector<Attribute> &attributes) const override {
        attributes.clear();
        attributes = this->attrs;


        // For attribute in std::vector<Attribute>, name it as rel.attr
        for (Attribute &attribute : attributes) {
            std::string tmp = tableName;
            tmp += ".";
            tmp += attribute.name;
            attribute.name = tmp;
        }
    };

    ~IndexScan() override {
        iter->close();
    };
};

class Filter : public Iterator {
    // Filter operator
public:
    Filter(Iterator *input,               // Iterator of input R
           const Condition &condition     // Selection condition
    );

    ~Filter() override = default;

    RC getNextTuple(void *data) override ;
    RC readAttribute(const std::vector<Attribute> &attrs, const std::string attrName, const void* tupleData, char* attrData, AttrType &attrType);
    // For attribute in std::vector<Attribute>, name it as rel.attr
    void getAttributes(std::vector<Attribute> &attrs) const override;
    uint32_t getAttributeMaxLength(std::vector<Attribute> &attrs, const std::string attrName);
    bool compValue(const char* lData, const char* conditionData);

    Iterator *m_input;
    Condition m_condition;
    AttrType m_attrType;
};

class Project : public Iterator {
    // Projection operator
public:
    Project(Iterator *input,                    // Iterator of input R
            const std::vector<std::string> &attrNames);   // std::vector containing attribute names
    ~Project() override = default;

    RC getNextTuple(void *data) override;

    // For attribute in std::vector<Attribute>, name it as rel.attr
    void getAttributes(std::vector<Attribute> &attrs) const override;

    Iterator *m_input;
    std::vector<Attribute> m_leftAttributes;
    std::vector<Attribute> m_projectedAttributes;
};

class BNLJoin : public Iterator {
    // Block nested-loop join operator
public:
    BNLJoin(Iterator *leftIn,            // Iterator of input R
            TableScan *rightIn,           // TableScan Iterator of input S
            const Condition &condition,   // Join condition
            const unsigned numPages       // # of pages that can be loaded into memory,
            //   i.e., memory block size (decided by the optimizer)
    );

    ~BNLJoin() override;

    RC getNextTuple(void *data) override;

    // For attribute in std::vector<Attribute>, name it as rel.attr
    void getAttributes(std::vector<Attribute> &attrs) const override;

    bool finishFlag;
    Iterator*  m_leftInput;
    TableScan* m_rightInput;
    Condition  m_condition;
    unsigned   m_numPages;
    unsigned   m_hashTableSize;
    unsigned  m_nextIndicator;
    // unsigned   m_numRecords;
    
    std::vector<Attribute> m_leftAttribute;
    std::vector<Attribute> m_rightAttribute;

    uint8_t* m_leftInputData;
    uint8_t* m_rightInputData;

    std::map<Key, std::vector<Record>> m_hashtable;
    std::vector<std::pair<Record, Record>> recordBuffer; 
};

class INLJoin : public Iterator {
    // Index nested-loop join operator
public:
    INLJoin(Iterator *leftIn,           // Iterator of input R
            IndexScan *rightIn,          // IndexScan Iterator of input S
            const Condition &condition   // Join condition
    );

    ~INLJoin() override;

    RC getNextTuple(void *data) override;

    // For attribute in std::vector<Attribute>, name it as rel.attr
    void getAttributes(std::vector<Attribute> &attrs) const override ;



    Iterator *m_leftInput;
    IndexScan *m_rightInput;

    std::vector<Attribute> m_leftAttribute;
    std::vector<Attribute> m_rightAttribute;

    char* m_leftTupleData;
    char* m_rightTupleData;

    Condition m_condition;
    bool m_isFirstScan;

};

// Optional for everyone. 10 extra-credit points
class GHJoin : public Iterator {
    // Grace hash join operator
public:
    GHJoin(Iterator *leftIn,               // Iterator of input R
           Iterator *rightIn,               // Iterator of input S
           const Condition &condition,      // Join condition (CompOp is always EQ)
           const unsigned numPartitions     // # of partitions for each relation (decided by the optimizer)
    ) {};

    ~GHJoin() override = default;

    RC getNextTuple(void *data) override { return QE_EOF; };

    // For attribute in std::vector<Attribute>, name it as rel.attr
    void getAttributes(std::vector<Attribute> &attrs) const override {};
};

class Aggregate : public Iterator {
    // Aggregation operator
public:
    // Mandatory
    // Basic aggregation
    Aggregate(Iterator *input,          // Iterator of input R
              const Attribute &aggAttr,        // The attribute over which we are computing an aggregate
              AggregateOp op            // Aggregate operation
    );

    // Optional for everyone: 5 extra-credit points
    // Group-based hash aggregation
    Aggregate(Iterator *input,             // Iterator of input R
              const Attribute &aggAttr,           // The attribute over which we are computing an aggregate
              const Attribute &groupAttr,         // The attribute over which we are grouping the tuples
              AggregateOp op              // Aggregate operation
    );

    ~Aggregate() override = default;

    RC getNextTuple(void *data) override;
    RC getNextTupleWithoutGroup(void *data);
    RC getNextTupleWithGroup(void *data);
    RC calculateGroupBy();

    // Please name the output attribute as aggregateOp(aggAttr)
    // E.g. Relation=rel, attribute=attr, aggregateOp=MAX
    // output attrname = "MAX(rel.attr)"
    void getAttributes(std::vector<Attribute> &attrs) const override;
    void updateComparatorIfNeeded(const void *tuple, char *comparator, std::string attrName, AggregateOp op);
    void updateCumulator(const void *tuple, char *cumulator, std::string attrName);

    std::string getAggrOpName(AggregateOp aggregateOp) const;

    Iterator *m_input;
    Attribute m_aggAttribute;
    Attribute m_groupAttr;
    AggregateOp m_aggreOp;
    std::vector<Attribute> m_attributes;
    bool m_end;

    bool groupFlag;
    std::map<Key, std::pair<char*,float>> groupValue;
    // std::map<Key, std::pair<char*,float>>::iterator groupIt;
    uint8_t* tupleData;
    float m_tupleNum;

};

class Utility {
public:
    static Attribute getAttributeByName(std::string attrName, std::vector<Attribute> attrs);
    static uint32_t getAttrIndexByName(std::string attrName, std::vector<Attribute> &attrs);
    static bool isNullByName(std::string attrName, const void *tuple, std::vector<Attribute> &attrs);
};

#endif
