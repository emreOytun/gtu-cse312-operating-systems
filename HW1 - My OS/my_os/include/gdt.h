#ifndef __MYOS__GDT_H
#define __MYOS__GDT_H

#include <common/types.h>

namespace myos
{
    
    class GlobalDescriptorTable
    {
        public:

            class SegmentDescriptor
            {
                private:
                    myos::common::uint16_t limit_lo;
                    myos::common::uint16_t base_lo;
                    myos::common::uint8_t base_hi;
                    myos::common::uint8_t type;
                    myos::common::uint8_t limit_hi;
                    myos::common::uint8_t base_vhi;

                public:
                    SegmentDescriptor();    
                    SegmentDescriptor(myos::common::uint32_t base, myos::common::uint32_t limit, myos::common::uint8_t type);
                    myos::common::uint32_t Base();
                    myos::common::uint32_t Limit();
            } __attribute__((packed));

        private:
            SegmentDescriptor nullSegmentSelector;
            SegmentDescriptor unusedSegmentSelector;
            SegmentDescriptor codeSegmentSelector;
            SegmentDescriptor dataSegmentSelector;
            
            
        public:

            GlobalDescriptorTable();
            ~GlobalDescriptorTable();

            /*
            /////////////////////////////////////////////////  TODO: YENI EKLENDI. SIMDILIK PUBLIC GEREKIRSE GETTER SETTER'I YAP BU SEKILDE
            SegmentDescriptor processCodeSegmentSelector[5]; // 256: MAX_PROCESS
            SegmentDescriptor processDataSegmentSelector[5];
            /////////////////////////////////////////////////
            */
            
            myos::common::uint16_t CodeSegmentSelector();
            myos::common::uint16_t DataSegmentSelector();
            
            /*
            ///////////////////////////////////////////////// TODO: YENI EKLENDI
            myos::common::uint16_t GetProcessCodeSegmentSelector(int processId);
            myos::common::uint16_t GetProcessDataSegmentSelector(int processId);
            /////////////////////////////////////////////////
            */
    };

}
    
#endif
