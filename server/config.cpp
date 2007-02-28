
/* Copyright (c) 2005-2007, Stefan Eilemann <eile@equalizergraphics.com> 
   All rights reserved. */

#include "config.h"

#include "compound.h"
#include "node.h"
#include "server.h"
#include "global.h"

#include <eq/net/command.h>
#include <eq/net/global.h>

using namespace eqs;
using namespace eqBase;
using namespace std;

#define MAKE_ATTR_STRING( attr ) ( string("EQ_CONFIG_") + #attr )
std::string Config::_fAttributeStrings[FATTR_ALL] = 
{
    MAKE_ATTR_STRING( FATTR_EYE_BASE )
};

void Config::_construct()
{
    _latency     = 1;
    _frameNumber = 0;
    _state       = STATE_STOPPED;
    _appNode     = 0;

    registerCommand( eq::CMD_CONFIG_INIT,
                     eqNet::CommandFunc<Config>( this, &Config::_cmdPush ));
    registerCommand( eq::REQ_CONFIG_INIT,
                     eqNet::CommandFunc<Config>( this, &Config::_reqInit ));
    registerCommand( eq::CMD_CONFIG_EXIT, 
                     eqNet::CommandFunc<Config>( this, &Config::_cmdPush ));
    registerCommand( eq::REQ_CONFIG_EXIT, 
                     eqNet::CommandFunc<Config>( this, &Config::_reqExit ));
    registerCommand( eq::CMD_CONFIG_START_FRAME, 
                     eqNet::CommandFunc<Config>( this, &Config::_cmdPush ));
    registerCommand( eq::REQ_CONFIG_START_FRAME,
                   eqNet::CommandFunc<Config>( this, &Config::_reqStartFrame ));
    registerCommand( eq::CMD_CONFIG_FINISH_FRAME, 
                     eqNet::CommandFunc<Config>( this, &Config::_cmdPush ));
    registerCommand( eq::REQ_CONFIG_FINISH_FRAME, 
                  eqNet::CommandFunc<Config>( this, &Config::_reqFinishFrame ));
    registerCommand( eq::CMD_CONFIG_FINISH_ALL_FRAMES, 
                     eqNet::CommandFunc<Config>( this, &Config::_cmdPush ));
    registerCommand( eq::REQ_CONFIG_FINISH_ALL_FRAMES, 
              eqNet::CommandFunc<Config>( this, &Config::_reqFinishAllFrames ));

    const Global* global = Global::instance();
    
    for( int i=0; i<FATTR_ALL; ++i )
        _fAttributes[i] = global->getConfigFAttribute( (FAttribute)i );
        
    EQINFO << "New config @" << (void*)this << endl;
}

Config::Config()
{
    _construct();
}

Config::~Config()
{
    EQINFO << "Delete config @" << (void*)this << endl;
    _server     = 0;
    _appNode    = 0;

    for( vector<Node*>::const_iterator i = _nodes.begin(); i != _nodes.end();
         ++i )
    {
        Node* node = *i;

        node->_config = 0;
        delete node;
    }
    _nodes.clear();

    for( vector<Compound*>::const_iterator i = _compounds.begin(); 
         i != _compounds.end(); ++i )
    {
        Compound* compound = *i;

        compound->_config = 0;
        delete compound;
    }
    _compounds.clear();
}

struct ReplaceChannelData
{
    Channel* oldChannel;
    Channel* newChannel;
};
static TraverseResult replaceChannelCB(Compound* compound, void* userData )
{
    ReplaceChannelData* data = (ReplaceChannelData*)userData;
                            
    if( compound->getChannel() == data->oldChannel )
        compound->setChannel( data->newChannel );
    
    return TRAVERSE_CONTINUE;
}

Config::Config( const Config& from )
        : eqNet::Session( eq::CMD_CONFIG_CUSTOM ),
          _server( from._server )
{
    _construct();
    _appNetNode = from._appNetNode;
    _latency    = from._latency;

    const uint32_t nCompounds = from.nCompounds();
    for( uint32_t i=0; i<nCompounds; ++i )
    {
        Compound* compound      = from.getCompound(i);
        Compound* compoundClone = new Compound( *compound );

        addCompound( compoundClone );
        // channel is replaced below
    }

    for( int i=0; i<FATTR_ALL; ++i )
        _fAttributes[i] = from.getFAttribute( (FAttribute)i );
        
    const uint32_t nNodes = from.nNodes();
    for( uint32_t i=0; i<nNodes; ++i )
    {
        eqs::Node* node      = from.getNode(i);
        eqs::Node* nodeClone = new eqs::Node( *node );
        
        (node == from._appNode) ? 
            addApplicationNode( nodeClone ) : addNode( nodeClone );

        // replace channels in compounds
        const uint32_t nPipes = node->nPipes();
        for( uint32_t j=0; j<nPipes; j++ )
        {
            Pipe* pipe      = node->getPipe(j);
            Pipe* pipeClone = nodeClone->getPipe(j);
            
            const uint32_t nWindows = pipe->nWindows();
            for( uint32_t k=0; k<nWindows; k++ )
            {
                Window* window      = pipe->getWindow(k);
                Window* windowClone = pipeClone->getWindow(k);
            
                const uint32_t nChannels = window->nChannels();
                for( uint32_t l=0; l<nChannels; l++ )
                {
                    Channel* channel      = window->getChannel(l);
                    Channel* channelClone = windowClone->getChannel(l);

                    ReplaceChannelData data;
                    data.oldChannel = channel;
                    data.newChannel = channelClone;

                    for( uint32_t m=0; m<nCompounds; m++ )
                    {
                        Compound* compound = getCompound(m);

                        Compound::traverse( compound, replaceChannelCB, 
                                            replaceChannelCB, 0, &data );
                    }
                }
            }
        }
    }
}

void Config::addNode( Node* node )
{
    _nodes.push_back( node ); 
    
    node->_config = this; 
}

bool Config::removeNode( Node* node )
{
    vector<Node*>::iterator iter = find( _nodes.begin(), _nodes.end(), node );
    if( iter == _nodes.end( ))
        return false;

    _nodes.erase( iter );
    node->_config = 0; 

    return true;
}

void Config::addCompound( Compound* compound )
{
    compound->_config = this;
    _compounds.push_back( compound );
}

bool Config::removeCompound( Compound* compound )
{
    vector<Compound*>::iterator i = find( _compounds.begin(), _compounds.end(),
                                          compound );
    if( i == _compounds.end( ))
        return false;

    _compounds.erase( i );
    compound->_config = 0;
    return true;
}

Channel* Config::findChannel( const std::string& name ) const
{
    const uint32_t nNodes = this->nNodes();
    for( uint32_t i=0; i<nNodes; ++i )
    {
        eqs::Node*     node   = getNode(i);
        const uint32_t nPipes = node->nPipes();
        for( uint32_t j=0; j<nPipes; j++ )
        {
            Pipe*          pipe     = node->getPipe(j);
            const uint32_t nWindows = pipe->nWindows();
            for( uint32_t k=0; k<nWindows; k++ )
            {
                Window*        window    = pipe->getWindow(k);
                const uint32_t nChannels = window->nChannels();
                for( uint32_t l=0; l<nChannels; l++ )
                {
                    Channel* channel = window->getChannel(l);
                    if( channel && channel->getName() == name )
                        return channel;
                }
            }
        }
    }
    return 0;
}

void Config::addApplicationNode( Node* node )
{
    EQASSERT( _state == STATE_STOPPED );
    EQASSERTINFO( !_appNode, "Only one application node per config possible" );

    _appNode = node;
    addNode( node );
}

void Config::setApplicationNode( eqBase::RefPtr<eqNet::Node> node )
{
    EQASSERT( _state == STATE_STOPPED );
    EQASSERT( !_appNetNode );

    _appNetNode = node;
}

//===========================================================================
// operations
//===========================================================================

//---------------------------------------------------------------------------
// init
//---------------------------------------------------------------------------
bool Config::_init( const uint32_t initID )
{
    EQASSERT( _state == STATE_STOPPED );
    _frameNumber = 0;

    const uint32_t nCompounds = this->nCompounds();
    for( uint32_t i=0; i<nCompounds; ++i )
    {
        Compound* compound = getCompound( i );
        compound->init();
    }

    if( !_connectNodes()  ||
        !_initNodes( initID ) ||
        !_initPipes( initID ) )
    {
        exit();
        return false;
    }

    _state = STATE_INITIALIZED;
    return true;
}

static RefPtr<eqNet::Node> _createNode( Node* node )
{
    RefPtr<eqNet::Node> netNode = new eqNet::Node;

    const uint32_t nConnectionDescriptions = node->nConnectionDescriptions();
    for( uint32_t i=0; i<nConnectionDescriptions; i++ )
    {
        eqNet::ConnectionDescription* desc = 
            node->getConnectionDescription(i).get();
        
        netNode->addConnectionDescription( 
            new eqNet::ConnectionDescription( *desc ));
    }

    return netNode;
}

bool Config::_connectNodes()
{
    RefPtr< eqNet::Node > localNode = getLocalNode();
    EQASSERT( localNode.isValid( ));

    for( NodeHashIter iter = _nodes.begin(); iter != _nodes.end(); ++iter )
    {
        Node* node = *iter;
        if( !node->isUsed( ))
            continue;

        RefPtr<eqNet::Node> netNode;

        if( node == _appNode )
            netNode = _appNetNode;
        else
        {
            netNode = _createNode( node );
            netNode->setAutoLaunch( true );
            netNode->setProgramName( _renderClient );
            netNode->setWorkDir( _workDir );
        }

        if( !localNode->initConnect( netNode ))
        {
            EQERROR << "Connection to " << netNode->getNodeID() << " failed." 
                    << endl;
            return false;
        }

        node->setNode( netNode );
    }
    return true;
}

bool Config::_initNodes( const uint32_t initID )
{
    const string& name    = getName();
    bool          success = true;

    RefPtr< eqNet::Node > localNode = getLocalNode();
    EQASSERT( localNode.isValid( ));

    eq::ServerCreateConfigPacket createConfigPacket;
    createConfigPacket.configID  = _id;
    createConfigPacket.appNodeID = _appNetNode->getNodeID();

    eq::ConfigCreateNodePacket createNodePacket;
    
    for( NodeHashIter iter = _nodes.begin(); iter != _nodes.end(); ++iter )
    {
        Node* node = *iter;
        if( !node->isUsed( ))
            continue;
        
        RefPtr<eqNet::Node> netNode = node->getNode();

        if( !localNode->syncConnect( netNode ))
        {
            EQERROR << "Connection of node " << netNode->getNodeID() 
                    << " failed." << endl;
            success = false;
        }
        
        if( !success ) 
            continue;

        // initialize nodes
        if( node != _appNode )
            netNode->send( createConfigPacket, name );

        registerObject( node );
        createNodePacket.nodeID = node->getID();
        send( netNode, createNodePacket );

        node->startConfigInit( initID );
    }

    for( NodeHashIter iter = _nodes.begin(); iter != _nodes.end(); ++iter )
    {
        Node*               node    = *iter;
        RefPtr<eqNet::Node> netNode = node->getNode();
        if( !node->isUsed() || !netNode->isConnected( ))
            continue;
        
        if( !node->syncConfigInit( ))
        {
            _error += ( ' ' + node->getErrorMessage( ));
            EQERROR << "Init of node " << (void*)node << " failed." 
                    << endl;
            success = false;
        }
    }
    return success;
}

bool Config::_initPipes( const uint32_t initID )
{
    // start pipe-window-channel init in parallel on all nodes
    for( NodeHashIter iter = _nodes.begin(); iter != _nodes.end(); ++iter )
    {
        Node* node = *iter;
        if( !node->isUsed( ))
            continue;

        // XXX move to node?
        eq::NodeCreatePipePacket createPipePacket;

        const vector<Pipe*>& pipes = node->getPipes();
        for( PipeIter iter = pipes.begin(); iter != pipes.end(); ++iter )
        {
            Pipe* pipe = *iter;
            if( !pipe->isUsed( ))
                continue;
            
            registerObject( pipe );
            createPipePacket.pipeID = pipe->getID();
            node->send( createPipePacket );
            pipe->startConfigInit( initID ); // recurses down
        }
    }

    // sync init of all pipe-window-channel entities
    bool success = true;
    for( NodeHashIter iter = _nodes.begin(); iter != _nodes.end(); ++iter )
    {
        Node* node = *iter;
        if( !node->isUsed( ))
            continue;
        
        const vector<Pipe*>& pipes = node->getPipes();
        for( PipeIter iter = pipes.begin(); iter != pipes.end(); ++iter )
        {
            Pipe* pipe = *iter;
            if( !pipe->isUsed( ))
                continue;

            if( !pipe->syncConfigInit( ))
            {
                _error += (' ' + pipe->getErrorMessage( ));
                success = false;
            }
        }
    }
    return success;
}

bool Config::exit()
{
    if( _state != STATE_INITIALIZED )
        return false;
//     foreach compound
//         call exit compound cb's
//     foreach compound
//         sync exit

    bool cleanExit = _exitPipes();
    if( !cleanExit )
        EQERROR << "pipes exit failed" << endl;

    if( !_exitNodes( ))
    {
        EQERROR << "nodes exit failed" << endl;
        cleanExit = false;
    }

    const uint32_t nCompounds = this->nCompounds();
    for( uint32_t i=0; i<nCompounds; ++i )
    {
        Compound* compound = getCompound( i );
        compound->exit();
    }

    if( _headMatrix.getID() != EQ_ID_INVALID )
        deregisterObject( &_headMatrix );

    _frameNumber = 0;
    _state       = STATE_STOPPED;
    return cleanExit;
}

bool Config::_exitPipes()
{
    // start pipe-window-channel exit in parallel
    for( NodeHashIter iter = _nodes.begin(); iter != _nodes.end(); ++iter )
    {
        Node* node = *iter;

        if( !node->isUsed( ))
            continue;
            
        const vector<Pipe*>& pipes = node->getPipes();
        for( PipeIter iter = pipes.begin(); iter != pipes.end(); ++iter )
        {
            Pipe* pipe = *iter;
            if( pipe->getState() == Pipe::STATE_STOPPED )
                continue;

            pipe->startConfigExit();
        }
    }

    // sync exit of all pipe-window-channel entities
    bool success = true;
    for( NodeHashIter iter = _nodes.begin(); iter != _nodes.end(); ++iter )
    {
        Node* node = *iter;
        if( !node->isUsed( ))
            continue;
        
        eq::NodeDestroyPipePacket destroyPipePacket;

        const vector<Pipe*>& pipes = node->getPipes();
        for( PipeIter iter = pipes.begin(); iter != pipes.end(); ++iter )
        {
            Pipe* pipe = *iter;
            if( pipe->getState() != Pipe::STATE_STOPPING )
                continue;

            if( !pipe->syncConfigExit( ))
            {
                EQWARN << "Could not exit cleanly: " << pipe << endl;
                success = false;
            }

            destroyPipePacket.pipeID = pipe->getID();
            node->send( destroyPipePacket );
            deregisterObject( pipe );
        }
    }

    return success;
}

bool Config::_exitNodes()
{
    vector<Node*> exitingNodes;
    for( NodeHashIter iter = _nodes.begin(); iter != _nodes.end(); ++iter )
    {
        Node* node = *iter;
        if( !node->isUsed() || 
            node->getNode()->getState() == eqNet::Node::STATE_STOPPED )
            continue;

        exitingNodes.push_back( node );
        node->startConfigExit();
    }

    eq::ServerDestroyConfigPacket destroyConfigPacket;
    destroyConfigPacket.configID  = _id;

    eq::ConfigDestroyNodePacket destroyNodePacket;
    eq::ClientExitPacket        clientExitPacket;

    bool success = true;
    for( vector<Node*>::const_iterator i = exitingNodes.begin();
         i != exitingNodes.end(); ++i )
    {
        Node*               node    = *i;
        RefPtr<eqNet::Node> netNode = node->getNode();

        if( !node->syncConfigExit( ))
        {
            EQERROR << "Could not exit cleanly: " << node << endl;
            success = false;
        }
        
        destroyNodePacket.nodeID = node->getID();
        send( netNode, destroyNodePacket );

        if( node != _appNode )
        {
            netNode->send( destroyConfigPacket );
            netNode->send( clientExitPacket );
        }

        node->setNode( 0 );
        deregisterObject( node );
    }
    return success;
}

void Config::_updateHead()
{
    _headMatrix.sync();
    const float         eyeBase_2 = .5f * getFAttribute(Config::FATTR_EYE_BASE);
    const eq::Matrix4f& head      = _headMatrix;

    // eye_world = (+-eye_base/2., 0, 0 ) x head_matrix
    // Don't use vector operator* due to possible simplification

    _eyePosition[EYE_INDEX_CYCLOP].x = head.m03;
    _eyePosition[EYE_INDEX_CYCLOP].y = head.m13;
    _eyePosition[EYE_INDEX_CYCLOP].z = head.m23;
    _eyePosition[EYE_INDEX_CYCLOP]  /= head.m33;

    _eyePosition[EYE_INDEX_LEFT].x = ( -eyeBase_2 * head.m00 + head.m03 );
    _eyePosition[EYE_INDEX_LEFT].y = ( -eyeBase_2 * head.m10 + head.m13 );
    _eyePosition[EYE_INDEX_LEFT].z = ( -eyeBase_2 * head.m20 + head.m23 );
    _eyePosition[EYE_INDEX_LEFT]  /= ( -eyeBase_2 * head.m30 + head.m33 ); // w

    _eyePosition[EYE_INDEX_RIGHT].x = ( eyeBase_2 * head.m00 + head.m03 );
    _eyePosition[EYE_INDEX_RIGHT].y = ( eyeBase_2 * head.m10 + head.m13 );
    _eyePosition[EYE_INDEX_RIGHT].z = ( eyeBase_2 * head.m20 + head.m23 );
    _eyePosition[EYE_INDEX_RIGHT]  /= ( eyeBase_2 * head.m30 + head.m33 ); // w
}

const vmml::Vector3f& Config::getEyePosition( const uint32_t eye )
{
    switch( eye )
    {
        case Compound::EYE_CYCLOP:
            return _eyePosition[EYE_INDEX_CYCLOP];
        case Compound::EYE_LEFT:
            return _eyePosition[EYE_INDEX_LEFT];
        case Compound::EYE_RIGHT:
            return _eyePosition[EYE_INDEX_RIGHT];
        default:
            EQERROR << "Unknown eye position" << endl;
            return _eyePosition[EYE_INDEX_CYCLOP];
    }
}

uint32_t Config::_prepareFrame( vector<Node*>& nodes )
{
    EQASSERT( _state == STATE_INITIALIZED );
    ++_frameNumber;
    EQLOG( LOG_ANY ) << "----- Start Frame ----- " << _frameNumber << endl;

    _updateHead();

    const uint32_t nNodes = this->nNodes();
    for( uint32_t i=0; i<nNodes; ++i )
    {
        Node* node = getNode( i );
        if( node->isUsed( ))
            nodes.push_back( node );
    }
    
    return _frameNumber;
}

void Config::_startFrame( const uint32_t frameID )
{
    EQASSERT( _state == STATE_INITIALIZED );

    const uint32_t nCompounds = this->nCompounds();
    for( uint32_t i=0; i<nCompounds; ++i )
    {
        Compound* compound = getCompound( i );
        compound->update( _frameNumber );
    }

    const uint32_t nNodes = this->nNodes();
    for( uint32_t i=0; i<nNodes; ++i )
    {
        Node* node = getNode( i );
        if( node->isUsed( ))
            node->startFrame( frameID, _frameNumber );
    }
}

uint32_t Config::_finishFrame()
{
    if( _frameNumber <= _latency )
        return 0;

    const uint32_t finishFrame = _frameNumber - _latency;
    const uint32_t nNodes = this->nNodes();
    for( uint32_t i=0; i<nNodes; ++i )
    {
        Node* node = getNode( i );
        if( node->isUsed( ))
            node->syncUpdate( finishFrame );
    }

    EQLOG( LOG_ANY ) << "----- Finish Frame ---- " << finishFrame << endl;
    return finishFrame;
}

uint32_t Config::_finishAllFrames()
{
    const uint32_t nNodes = this->nNodes();
    for( uint32_t i=0; i<nNodes; ++i )
    {
        Node* node = getNode( i );
        if( node->isUsed( ))
            node->syncUpdate( _frameNumber );
    }

    EQLOG( LOG_ANY ) << "-- Finish All Frames -- " << _frameNumber << endl;
    return _frameNumber;
}

//---------------------------------------------------------------------------
// command handlers
//---------------------------------------------------------------------------
eqNet::CommandResult Config::_reqInit( eqNet::Command& command )
{
    const eq::ConfigInitPacket* packet = 
        command.getPacket<eq::ConfigInitPacket>();
    eq::ConfigInitReplyPacket   reply( packet );
    EQINFO << "handle config init " << packet << endl;

    _error.clear();
    reply.result = _init( packet->initID );

    if( reply.result )
        mapObject( &_headMatrix, packet->headMatrixID );

    EQINFO << "Config init " << (reply.result ? "successful":"failed: ") 
           << _error << endl;
    send( command.getNode(), reply, _error );
    return eqNet::COMMAND_HANDLED;
}

eqNet::CommandResult Config::_reqExit( eqNet::Command& command ) 
{
    const eq::ConfigExitPacket* packet = 
        command.getPacket<eq::ConfigExitPacket>();
    eq::ConfigExitReplyPacket   reply( packet );
    EQINFO << "handle config exit " << packet << endl;

    reply.result = exit();
    EQINFO << "config exit result: " << reply.result << endl;
    send( command.getNode(), reply );
    return eqNet::COMMAND_HANDLED;
}

eqNet::CommandResult Config::_reqStartFrame( eqNet::Command& command ) 
{
    const eq::ConfigStartFramePacket* packet = 
        command.getPacket<eq::ConfigStartFramePacket>();
    eq::ConfigStartFrameReplyPacket   reply( packet );
    EQVERB << "handle config frame start " << packet << endl;

    vector<Node*> nodes;
    reply.frameNumber = _prepareFrame( nodes );
    reply.nNodeIDs    = nodes.size();
    command.getNode()->send( reply, nodes );

    _startFrame( packet->frameID );
    return eqNet::COMMAND_HANDLED;
}

eqNet::CommandResult Config::_reqFinishFrame( eqNet::Command& command ) 
{
    const eq::ConfigFinishFramePacket* packet = 
        command.getPacket<eq::ConfigFinishFramePacket>();
    eq::ConfigFinishFrameReplyPacket   reply( packet );
    EQVERB << "handle config frame finish " << packet << endl;

    reply.result = _finishFrame();
    send( command.getNode(), reply );
    return eqNet::COMMAND_HANDLED;
}

eqNet::CommandResult Config::_reqFinishAllFrames( eqNet::Command& command ) 
{
    const eq::ConfigFinishAllFramesPacket* packet = 
        command.getPacket<eq::ConfigFinishAllFramesPacket>();
    eq::ConfigFinishAllFramesReplyPacket   reply( packet );
    EQVERB << "handle config all frames finish " << packet << endl;

    reply.result = _finishAllFrames();
    send( command.getNode(), reply );
    return eqNet::COMMAND_HANDLED;
}


ostream& eqs::operator << ( ostream& os, const Config* config )
{
    if( !config )
        return os;

    os << disableFlush << disableHeader << "config " << endl;
    os << "{" << endl << indent;

    if( config->getLatency() != 1 )
        os << "latency " << config->getLatency() << endl;
    os << endl;

    const float value = config->getFAttribute( Config::FATTR_EYE_BASE );
    if( value != 
        Global::instance()->getConfigFAttribute( Config::FATTR_EYE_BASE ))
    {
        os << "attributes" << endl << "{" << endl << indent;
        os << "eyeBase " << value << endl;
        os << exdent << "}" << endl;
    }

    const uint32_t nNodes = config->nNodes();
    for( uint32_t i=0; i<nNodes; ++i )
        os << config->getNode(i);

    const uint32_t nCompounds = config->nCompounds();
    for( uint32_t i=0; i<nCompounds; ++i )
        os << config->getCompound(i);

    os << exdent << "}" << endl << enableHeader << enableFlush;

    return os;
}
