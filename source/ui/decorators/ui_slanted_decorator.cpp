#include "ui_precompiled.h"
#include "kernel/ui_common.h"
#include "kernel/ui_main.h"

namespace WSWUI {

typedef Rocket::Core::Element Element;
typedef Rocket::Core::Decorator Decorator;
typedef Rocket::Core::DecoratorDataHandle DecoratorDataHandle;
typedef Rocket::Core::DecoratorInstancer DecoratorInstancer;
typedef Rocket::Core::PropertyDictionary PropertyDictionary;
typedef Rocket::Core::Colourb Colourb;

/*
    Usage in CSS:

        slant-decorator: slanted;
        slant-color: #00FF00;
        slant-angle: +45;
*/
class SlantedDecorator final: public Decorator {
	Colourb color;
	float tangent;

	static const float DEFAULT_TANGENT;
public:
	static constexpr float DEFAULT_DEGREES = 20;

	explicit SlantedDecorator( const PropertyDictionary &properties ) {
		color = properties.GetProperty( "color" )->Get<Colourb>();
		float degrees = properties.GetProperty( "angle" )->Get<float>();
		if( degrees == DEFAULT_DEGREES ) {
			tangent = DEFAULT_TANGENT;
			return;
		} else {
			// Values outside this range do not really make sense
			clamp( degrees, -60.0f, 60.0f );
			tangent = ::tanf( DEG2RAD( degrees ) );
		}
	}

	DecoratorDataHandle GenerateElementData( Element *element ) override {
		return 0;
	}

	void ReleaseElementData( DecoratorDataHandle element_data ) override {
	}

	void RenderElement( Element *element, DecoratorDataHandle element_data ) override {
		typedef Rocket::Core::Vertex Vertex;
		typedef Rocket::Core::Vector2f Vector2f;

		Vector2f topleft = Vector2f( element->GetAbsoluteLeft() + element->GetClientLeft(),
									 element->GetAbsoluteTop() + element->GetClientTop() );
		Vector2f bottomright = Vector2f( topleft.x + element->GetClientWidth(), topleft.y + element->GetClientHeight() );

		// create the renderable vertexes
		Vertex vertex[4];
		for( int i = 0; i < 4; i++ ) {
			vertex[i].tex_coord = Vector2f( 0.0f, 0.0f );
			vertex[i].colour = this->color;
		}

		const float shift = 0.5f * element->GetClientHeight() * tangent;

		vertex[0].position = topleft;
		vertex[0].position.x += shift;
		vertex[1].position = Vector2f( bottomright.x + shift, topleft.y );
		vertex[2].position = bottomright;
		vertex[2].position.x -= shift;
		vertex[3].position = Vector2f( topleft.x - shift, bottomright.y );

		int indices[6] = { 0, 1, 2, 0, 2, 3 };

		Rocket::Core::RenderInterface *renderer = element->GetRenderInterface();
		renderer->RenderGeometry( vertex, 4, indices, 6, 0, Vector2f( 0.0, 0.0 ) );
	}
};

const float SlantedDecorator::DEFAULT_TANGENT = ::tanf( DEG2RAD( SlantedDecorator::DEFAULT_DEGREES ) );

class SlantedDecoratorInstancer: public DecoratorInstancer {
public:
	SlantedDecoratorInstancer() {
		RegisterProperty( "color", "#ffff" ).AddParser( "color" );
		RegisterProperty( "angle", va( "%f", SlantedDecorator::DEFAULT_DEGREES ) ).AddParser( "number" );
	}

	Decorator* InstanceDecorator( const String& name, const PropertyDictionary& _properties ) override {
		return __new__( SlantedDecorator )( _properties );
	}

	void ReleaseDecorator( Decorator* decorator ) override {
		__delete__( decorator );
	}

	void Release() override {
		__delete__( this );
	}
};

DecoratorInstancer *GetSlantedDecoratorInstancer() {
	return __new__( SlantedDecoratorInstancer );
}

}
