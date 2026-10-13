/**
 * @file test_virtual.cpp
 * @brief Test file.
 * @details Tests functionality.
 */
/**
 * @brief Base1 class.
 * @details Helper class for testing.
 */
class Base1 {
public:
/**
 * @brief get1 function.
 * @details Auto-generated documentation.
 * @return Return value.
 */
    virtual int get1() { return 10; }
/**
 * @brief ~Base1 function.
 * @details Auto-generated documentation.
 * @return none.
 */
    virtual ~Base1() {}
};

/**
 * @brief Base2 class.
 * @details Helper class for testing.
 */
class Base2 {
public:
/**
 * @brief get2 function.
 * @details Auto-generated documentation.
 * @return Return value.
 */
    virtual int get2() { return 20; }
/**
 * @brief ~Base2 function.
 * @details Auto-generated documentation.
 * @return none.
 */
    virtual ~Base2() {}
};

/**
 * @brief DerivedMI class.
 * @details Helper class for testing.
 */
class DerivedMI : public Base1, public Base2 {
public:
/**
 * @brief get1 function.
 * @details Auto-generated documentation.
 * @return Return value.
 */
    int get1() override { return 11; }
/**
 * @brief get2 function.
 * @details Auto-generated documentation.
 * @return Return value.
 */
    int get2() override { return 22; }
};

/**
 * @brief VBase class.
 * @details Helper class for testing.
 */
class VBase {
public:
/**
 * @brief vget function.
 * @details Auto-generated documentation.
 * @return Return value.
 */
    virtual int vget() { return 30; }
};

/**
 * @brief VDerived class.
 * @details Helper class for testing.
 */
class VDerived : virtual public VBase {
public:
/**
 * @brief vget function.
 * @details Auto-generated documentation.
 * @return Return value.
 */
    int vget() override { return 33; }
};

/**
 * @brief RetBase class.
 * @details Helper class for testing.
 */
class RetBase {
public:
/**
 * @brief get_ptr function.
 * @details Auto-generated documentation.
 * @return Return value.
 */
    virtual Base1* get_ptr() { return nullptr; }
};

/**
 * @brief RetDerived class.
 * @details Helper class for testing.
 */
class RetDerived : public RetBase {
public:
/**
 * @brief get_ptr function.
 * @details Auto-generated documentation.
 * @return Return value.
 */
    virtual DerivedMI* get_ptr() override { return new DerivedMI(); }
};

/**
 * @brief Main entry point.
 * @details Runs tests.
 * @return 0 on success.
 */
int main() {
    /** @brief dmi member. */
    DerivedMI dmi;
    Base1* b1 = &dmi;
    Base2* b2 = &dmi;
    
    if (b1->get1() != 11) return 1;
    if (b2->get2() != 22) return 2; // Tests this adjustment thunk for Base2
    
    /** @brief vd member. */
    VDerived vd;
    VBase* vb = &vd;
    if (vb->vget() != 33) return 3; // Tests vcall thunk adjustment
    
    /** @brief rd member. */
    RetDerived rd;
    RetBase* rb = &rd;
    Base1* ptr = rb->get_ptr(); // Tests covariant return adjustment thunk
    if (!ptr || ptr->get1() != 11) return 4;
    /** @brief ptr member. */
    delete ptr;
    
    return 0;
}
