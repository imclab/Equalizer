
/* Copyright (c) 2009-2011, Maxim Makhinya <maxmah@gmail.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * - Redistributions of source code must retain the above copyright notice, this
 *   list of conditions and the following disclaimer.
 * - Redistributions in binary form must reproduce the above copyright notice,
 *   this list of conditions and the following disclaimer in the documentation
 *   and/or other materials provided with the distribution.
 * - Neither the name of Eyescale Software GmbH nor the names of its
 *   contributors may be used to endorse or promote products derived from this
 *   software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include <eq/client/asyncRB/asyncRBThread.h>

#include <eq/eq.h>
#include <eq/client/system.h>
#ifdef AGL
#  include <eq/client/asyncRB/aglWindowShared.h>
#endif
#ifdef GLX
#  include <eq/client/asyncRB/glXWindowShared.h>
#endif

namespace eq
{

namespace
{
eq::SystemWindow* _initSharedContextWindow( eq::Window* wnd )
{
    EQASSERT( wnd );

    // store old drawable of window and set window's drawable to FBO,
    // create another (shared) osWindow and restore original drowable
    const int32_t drawable =
        wnd->getIAttribute( eq::Window::IATTR_HINT_DRAWABLE );
    wnd->setIAttribute( eq::Window::IATTR_HINT_DRAWABLE, eq::FBO );

    const int32_t stencil =
        wnd->getIAttribute( eq::Window::IATTR_PLANES_STENCIL );
    wnd->setIAttribute( eq::Window::IATTR_PLANES_STENCIL, eq::OFF );

    const eq::Pipe* pipe = wnd->getPipe();
    EQASSERT( pipe );

    eq::SystemWindow* sharedContextWindow = 0;

    const std::string& ws = pipe->getWindowSystem().getName();

#ifdef GLX
    if( ws == "GLX" )
    {
        EQINFO << "Using GLXWindow" << std::endl;
        sharedContextWindow = new GLXWindowShared( wnd );
    }
#endif
#ifdef AGL
    if( ws == "AGL" )
    {
        EQINFO << "Using AGLWindow" << std::endl;
        sharedContextWindow = new AGLWindowShared( wnd );
    }
#endif
#ifdef WGL
    if( ws == "WGL" )
    {
        EQINFO << "Using WGLWindow" << std::endl;
        sharedContextWindow = new eq::wgl::Window( wnd );
    }
#endif
    if( !sharedContextWindow )
    {
        EQERROR << "Window system " << pipe->getWindowSystem()
                << " not implemented or supported" << std::endl;
        return 0;
    }

    if( !sharedContextWindow->configInit( ))
    {
        EQWARN << "OS Window initialization failed: " << std::endl;
        delete sharedContextWindow;
        return 0;
    }

    wnd->setIAttribute( eq::Window::IATTR_HINT_DRAWABLE, drawable );
    wnd->setIAttribute( eq::Window::IATTR_PLANES_STENCIL, stencil );

    sharedContextWindow->makeCurrent();

    EQWARN << "Async fetcher initialization finished" << std::endl;
    return sharedContextWindow;
}


void _deleteSharedContextWindow( eq::Window* wnd,
                                 eq::SystemWindow** sharedContextWindow )
{
    EQWARN << "Deleting shared context" << std::endl;
    if( !sharedContextWindow || !*sharedContextWindow )
        return;

    const int32_t drawable =
        wnd->getIAttribute( eq::Window::IATTR_HINT_DRAWABLE );
    wnd->setIAttribute( eq::Window::IATTR_HINT_DRAWABLE, eq::FBO );

    (*sharedContextWindow)->configExit(); // mb set window to 0 before that?

    delete *sharedContextWindow;
    *sharedContextWindow = 0;

    wnd->setIAttribute( eq::Window::IATTR_HINT_DRAWABLE, drawable );
}
}

AsyncRBThread::AsyncRBThread()
    : eq::Worker()
    , _running( true )
    , _wnd( 0 )
    , _sharedContextWindow( 0 )
{
}


AsyncRBThread::~AsyncRBThread()
{
    if( _wnd && _sharedContextWindow )
        _deleteSharedContextWindow( _wnd, &_sharedContextWindow );
}


const GLEWContext* AsyncRBThread::glewGetContext() const
{
    return _sharedContextWindow->glewGetContext();
}


void AsyncRBThread::deleteSharedContextWindow()
{
    _deleteSharedContextWindow( _wnd, &_sharedContextWindow );
}


bool AsyncRBThread::init()
{
    EQASSERT( !_sharedContextWindow );
    if( !_wnd )
        return false;

    _sharedContextWindow = _initSharedContextWindow( _wnd );
    if( _sharedContextWindow )
        return true;

    EQERROR << "Can't init shader context window" << std::endl;
    return false;
}

} //namespace eq
