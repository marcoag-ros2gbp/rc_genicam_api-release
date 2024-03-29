//-----------------------------------------------------------------------------
//  (c) 2006 by Basler Vision Technologies
//  Section: Vision Components
//  Project: GenApi
//  Author:  Alexander Happe
//
//  License: This file is published under the license of the EMVA GenICam Standard Group.
//  A text file describing the legal terms is included in your installation as 'GenICam_license.pdf'.
//  If for some reason you are missing this file please contact the EMVA or visit the website
//  (http://www.genicam.org) for a full copy.
//
//  THIS SOFTWARE IS PROVIDED BY THE EMVA GENICAM STANDARD GROUP "AS IS"
//  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
//  THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
//  PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE EMVA GENICAM STANDARD GROUP
//  OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
//  EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
//  PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
//  OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
//  WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
//  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
//  POSSIBILITY OF SUCH DAMAGE.
//-----------------------------------------------------------------------------
/*!
\file
\brief    Definition of the RegisterT class template
\ingroup GenApi_Implementation
*/

#ifndef GENAPI_REGISTERT_H
#define GENAPI_REGISTERT_H

#include "../IRegister.h"
#include "GenApi/impl/Log.h"
#include "Exception.h"
#include "GenApi/Compatibility.h"

#ifndef _WIN32
#  define _snprintf snprintf
#endif

namespace GENAPI_NAMESPACE
{

#   ifdef _MSC_VER
#       pragma warning ( push )
#       pragma warning ( disable : 4996 ) // deprecated function
#   endif
    /**
        \brief Implementation of the IRegister interface
    */
    template <class Base>
    class RegisterT
        : public Base
    {
    public:

        //! Implementation of IRegister::Set()
        virtual void Set(const uint8_t *pBuffer, int64_t Length, bool Verify = true)
        {
            // a list of callbacks to fire held outside(!) the autolock on the stack(!)
            std::list<CNodeCallback*> CallbacksToFire;
            {
                AutoLock l(Base::GetLock());
                typename Base::EntryMethodFinalizer E(this, meSet, Base::IsStreamable());

                // logging the set
                if (Base::m_pValueLog && GENICAM_NAMESPACE::CLog::Exist( "" ))
                {
                    if (pBuffer)
                    {
                        static const char fmt[] =
                            "Set( %" FMT_I64 "d, 0x";
                        static const int BufferLen(256);
                        char _pBuffer[256];

                        int BufferLeft(_snprintf(_pBuffer, BufferLen, fmt, Length));

                        /* MANTIS 0000062 */
                        for(int i = 0; i < Length; i++)
                        {
                            const int n =_snprintf(_pBuffer + BufferLeft,
                                                   BufferLen - BufferLeft,
                                                   "%02X", (unsigned int) pBuffer[i]);
#pragma BullseyeCoverage off
                            #ifdef _MSC_VER
                            if (n < 0)
                            #else
                            if (BufferLeft + n >= BufferLen)
                            #endif
                                break;
#pragma BullseyeCoverage on
                            BufferLeft += n;
                        }
                        Base::m_pValueLog->Log( GENICAM_NAMESPACE::ILogger::INFO, "%s)   ", _pBuffer );
                        GENICAM_NAMESPACE::CLog::PushIndent();
                    }
                }

                if (!Base::CanBeWritten( Verify ))
                {
                    throw  ACCESS_EXCEPTION_NODE( "Node is not writable" );
                }

                {
                    typename Base::PostSetValueFinalizer PostSetValueCaller(this, CallbacksToFire);  // dtor calls Base::PostSetValue

                    Base::PreSetValue(); // invalidates all nodes if this is the first call in a chain of SetValue like calls
                    Base::InternalSet(pBuffer, Length, Verify);

                }
                if (Verify)
                {
                    Base::InternalCheckError();
                }

                GCLOGINFOPOP( Base::m_pValueLog, "...Set" );

                // fire callbacks inside the lock
                std::list<CNodeCallback*>::iterator ptrCallback;
                for( ptrCallback = CallbacksToFire.begin(); ptrCallback != CallbacksToFire.end(); ptrCallback++ )
                {
                    (*ptrCallback)->operator ()(cbPostInsideLock);
                }
            }

            // fire callbacks outside the lock
            std::list<CNodeCallback*>::iterator ptrCallback;
            for( ptrCallback = CallbacksToFire.begin(); ptrCallback != CallbacksToFire.end(); ptrCallback++ )
            {
                (*ptrCallback)->operator ()(cbPostOutsideLock);
            }
        }


        //! Implementation of IRegister::Get()
        virtual void Get(uint8_t *pBuffer, int64_t Length, bool Verify = false, bool IgnoreCache = false)
        {
            AutoLock l(Base::GetLock());
            typename Base::EntryMethodFinalizer E(this, meGet, Base::IsStreamable(), IgnoreCache);

            GCLOGINFOPUSH( Base::m_pValueLog, "Get...");

            // Note that readability is tested regardless of Verify
            if( !IsReadable( this ) )
                throw ACCESS_EXCEPTION_NODE("Node is not readable");

            Base::InternalGet(pBuffer, Length, Verify, IgnoreCache );

            if (Verify)
            {
                Base::InternalCheckError();
            }


            if (Base::m_pValueLog && GENICAM_NAMESPACE::CLog::Exist(""))
            {
                static const char fmt[] =
                    "...Get( %" FMT_I64 "d ) = 0x";

                static const int BufferLen( 256 );
                char _pBuffer[256];
                int BufferLeft( _snprintf( _pBuffer, BufferLen, fmt, Length ) );

                /* MANTIS 0000062 */
                for (int i = 0; i < Length; i++)
                {
                    const int n = _snprintf( _pBuffer + BufferLeft,
                                             BufferLen - BufferLeft,
                                             "%02X", (unsigned int) pBuffer[i] );
#pragma BullseyeCoverage off
#ifdef _MSC_VER
                    if (n < 0)
                        break;
#else
                    if (BufferLeft + n >= BufferLen)
                        break;
#endif
#pragma BullseyeCoverage on
                    BufferLeft += n;
                }
                Base::m_pValueLog->Log( GENICAM_NAMESPACE::ILogger::INFO, "%s", _pBuffer );
                GENICAM_NAMESPACE::CLog::PopIndent();
            }
        }

        //! Implementation of IRegister::GetLength()
        virtual int64_t GetLength( bool Verify = false )
        {
            AutoLock l(Base::GetLock());

            return Base::InternalGetLength(Verify);
        }

        //! Retrieves the Address of the register
        virtual int64_t GetAddress( bool Verify = false )
        {
            AutoLock l(Base::GetLock());

            return Base::InternalGetAddress(Verify,false);
        }

    };
#   ifdef _MSC_VER
#       pragma warning ( pop )
#   endif

} // namespace GenApi

#endif // GENAPI_REGISTERT_H
