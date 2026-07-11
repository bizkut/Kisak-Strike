#ifndef KISAK_PS4_GNM_VERTEX_DECLARATION_H
#define KISAK_PS4_GNM_VERTEX_DECLARATION_H

#include <stdint.h>
#include <gnm.h>

class CPs4GnmVertexDeclaration
{
public:
    enum { kMaxElements = 16 };

    struct Element
    {
        uint8_t stream;
        uint16_t offset;
        GnmDataFormat format;
    };

    CPs4GnmVertexDeclaration();
    bool Initialize( const Element *elements, uint32_t count );
    void Reset();

    bool IsValid() const { return m_count != 0; }
    uint32_t ElementCount() const { return m_count; }
    const Element &GetElement( uint32_t index ) const { return m_elements[index]; }

private:
    Element m_elements[kMaxElements];
    uint32_t m_count;
};

#endif
