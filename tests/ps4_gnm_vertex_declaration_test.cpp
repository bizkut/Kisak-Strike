#include "ps4_gnm_vertex_declaration.h"

#include <assert.h>

int main()
{
    CPs4GnmVertexDeclaration declaration;
    assert( !declaration.Initialize( 0, 0 ) );
    const CPs4GnmVertexDeclaration::Element elements[] = {
        { 0, 0, GNM_FMT_R32G32B32A32_FLOAT },
        { 0, 16, GNM_FMT_R32G32B32A32_FLOAT }
    };
    assert( declaration.Initialize( elements, 2 ) );
    assert( declaration.IsValid() && declaration.ElementCount() == 2 );
    assert( declaration.GetElement( 1 ).offset == 16 );

    CPs4GnmVertexDeclaration::Element invalid = elements[0];
    invalid.stream = 8;
    assert( !declaration.Initialize( &invalid, 1 ) );
    declaration.Reset();
    assert( !declaration.IsValid() );
    return 0;
}
