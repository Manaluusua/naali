// For conditions of distribution and use, see copyright notice in license.txt

#include "StableHeaders.h"
#include "Primitive.h"
#include "RexNetworkUtils.h"
#include "RexLogicModule.h"
#include "EC_OpenSimPrim.h"
#include "EC_NetworkPosition.h"
#include "EC_Viewable.h"
#include "../OgreRenderingModule/EC_OgrePlaceable.h"
#include "../OgreRenderingModule/EC_OgreMesh.h"
#include "../OgreRenderingModule/OgreMeshResource.h"
#include "../OgreRenderingModule/OgreTextureResource.h"
#include "../OgreRenderingModule/OgreMaterialUtils.h"
#include "../OgreRenderingModule/Renderer.h"
#include "ConversionUtils.h"
#include "QuatUtils.h"
#include "SceneEvents.h"
#include "ResourceInterface.h"

namespace RexLogic
{

    Primitive::Primitive(RexLogicModule *rexlogicmodule)
    {
        rexlogicmodule_ = rexlogicmodule;
    }

    Primitive::~Primitive()
    {
    }

    Scene::EntityPtr Primitive::GetOrCreatePrimEntity(Core::entity_id_t entityid, const RexUUID &fullid)
    {
        // Make sure scene exists
        Scene::ScenePtr scene = rexlogicmodule_->GetCurrentActiveScene();
        if (!scene)
            return Scene::EntityPtr();
          
        Scene::EntityPtr entity = rexlogicmodule_->GetPrimEntity(entityid);
        if (!entity)
        {
            // Create a new entity.
            Scene::EntityPtr entity = CreateNewPrimEntity(entityid);
            rexlogicmodule_->RegisterFullId(fullid,entityid); 
            EC_OpenSimPrim &prim = *checked_static_cast<EC_OpenSimPrim*>(entity->GetComponent(EC_OpenSimPrim::NameStatic()).get());
            prim.LocalId = entityid; ///\note In current design it holds that localid == entityid, but I'm not sure if this will always be so?
            prim.FullId = fullid;
            CheckPendingRexPrimData(entityid);
            return entity;
        }

        // Send the 'Entity Updated' event.
        /*
        Core::event_category_id_t cat_id = framework_->GetEventManager()->QueryEventCategory("Scene");
        Foundation::ComponentInterfacePtr component = entity->GetComponent(EC_OpenSimPrim::NameStatic());
        EC_OpenSimPrim *prim = checked_static_cast<RexLogic::EC_OpenSimPrim *>(component.get());
        Scene::SceneEventData::Events entity_event_data(entityid);
        entity_event_data.sceneName = scene->Name();
        framework_->GetEventManager()->SendEvent(cat_id, Scene::Events::EVENT_ENTITY_UPDATED, &entity_event_data);
        */
        return entity;
    }  

    Scene::EntityPtr Primitive::CreateNewPrimEntity(Core::entity_id_t entityid)
    {
        Scene::ScenePtr scene = rexlogicmodule_->GetCurrentActiveScene();
        if (!scene)
            return Scene::EntityPtr();
        
        Core::StringVector defaultcomponents;
        defaultcomponents.push_back(EC_OpenSimPrim::NameStatic());
        defaultcomponents.push_back(EC_NetworkPosition::NameStatic());
        defaultcomponents.push_back(OgreRenderer::EC_OgrePlaceable::NameStatic());
                
        Scene::EntityPtr entity = scene->CreateEntity(entityid,defaultcomponents); 

        DebugCreateOgreBoundingBox(rexlogicmodule_, entity->GetComponent(OgreRenderer::EC_OgrePlaceable::NameStatic()),"AmbientRed");
        return entity;
    }    

    bool Primitive::HandleOSNE_ObjectUpdate(OpenSimProtocol::NetworkEventInboundData* data)
    {
        NetInMessage *msg = data->message;
 
        msg->ResetReading();
        uint64_t regionhandle = msg->ReadU64();
        msg->SkipToNextVariable(); // TimeDilation U16 ///\todo Unhandled inbound variable 'TimeDilation'.
        
        // Variable block: Object Data
        size_t instance_count = data->message->ReadCurrentBlockInstanceCount();
        for(size_t i = 0; i < instance_count; ++i)
        {
            uint32_t localid = msg->ReadU32(); 
            msg->SkipToNextVariable();		// State U8 ///\todo Unhandled inbound variable 'State'.
            RexUUID fullid = msg->ReadUUID();
            msg->SkipToNextVariable();		// CRC U32 ///\todo Unhandled inbound variable 'CRC'.
            uint8_t pcode = msg->ReadU8();

            Scene::EntityPtr entity = GetOrCreatePrimEntity(localid, fullid);
            EC_OpenSimPrim &prim = *checked_static_cast<EC_OpenSimPrim*>(entity->GetComponent(EC_OpenSimPrim::NameStatic()).get());
            EC_NetworkPosition &netpos = *checked_static_cast<EC_NetworkPosition*>(entity->GetComponent(EC_NetworkPosition::NameStatic()).get());

            ///\todo Are we setting the param or looking up by this param? I think the latter, but this is now doing the former. 
            ///      Will cause problems with multigrid support.
            prim.RegionHandle = regionhandle;

            prim.Material = msg->ReadU8();
            prim.ClickAction = msg->ReadU8();

            Core::Vector3Df ogre_scale = Core::OpenSimToOgreCoordinateAxes(msg->ReadVector3());
            // Scale is not handled by interpolation system, so set directly
            Foundation::ComponentPtr placeable = entity->GetComponent(OgreRenderer::EC_OgrePlaceable::NameStatic());  
            if (placeable)
            {
                OgreRenderer::EC_OgrePlaceable &ogrepos = *checked_static_cast<OgreRenderer::EC_OgrePlaceable*>(placeable.get());                                    
                ogrepos.SetScale(ogre_scale);
            }            
            
            size_t bytes_read = 0;
            const uint8_t *objectdatabytes = msg->ReadBuffer(&bytes_read);
            if (bytes_read == 60)
            {
                Core::Vector3Df ogre_pos;
                Core::Quaternion ogre_quat;

                // The data contents:
                // ofs  0 - pos xyz - 3 x float (3x4 bytes)
                // ofs 12 - vel xyz - 3 x float (3x4 bytes)
                // ofs 24 - acc xyz - 3 x float (3x4 bytes)
                // ofs 36 - orientation, quat with last (w) component omitted - 3 x float (3x4 bytes)
                // ofs 48 - angular velocity - 3 x float (3x4 bytes)
                // total 60 bytes
                
                netpos.position_ = Core::OpenSimToOgreCoordinateAxes(*reinterpret_cast<const Core::Vector3df*>(&objectdatabytes[0]));                
                netpos.velocity_ = Core::OpenSimToOgreCoordinateAxes(*reinterpret_cast<const Core::Vector3df*>(&objectdatabytes[12])); 
                netpos.accel_ = Core::OpenSimToOgreCoordinateAxes(*reinterpret_cast<const Core::Vector3df*>(&objectdatabytes[24]));
                netpos.rotation_ = Core::OpenSimToOgreQuaternion(Core::UnpackQuaternionFromFloat3((float*)&objectdatabytes[36])); 
                netpos.rotvel_ = Core::OpenSimToOgreCoordinateAxes(*reinterpret_cast<const Core::Vector3df*>(&objectdatabytes[48]));
                netpos.Updated();
            }
            else
                RexLogicModule::LogError("Error reading ObjectData for prim:" + Core::ToString(prim.LocalId) + ". Bytes read:" + Core::ToString(bytes_read));
            
            prim.ParentId = msg->ReadU32();
            prim.UpdateFlags = msg->ReadU32();
            
            // Skip path related variables
            msg->SkipToFirstVariableByName("Text");
            prim.HoveringText = msg->ReadString(); 
            msg->SkipToNextVariable();      // TextColor
            prim.MediaUrl = msg->ReadString();
        }
        return false;
    }
    
    void Primitive::HandleTerseObjectUpdateForPrim_60bytes(const uint8_t* bytes)
    {
        // The data contents:
        // ofs  0 - localid - packed to 4 bytes
        // ofs  4 - state
        // ofs  5 - 0
        // ofs  6 - position xyz - 3 x float (3x4 bytes)
        // ofs 18 - velocity xyz - packed to 6 bytes        
        // ofs 24 - acceleration xyz - packed to 6 bytes           
        // ofs 30 - rotation - packed to 8 bytes 
        // ofs 38 - rotational vel - packed to 6 bytes
        
        //! \todo handle endians
        int i = 0;
        uint32_t localid = *reinterpret_cast<uint32_t*>((uint32_t*)&bytes[i]);                
        i += 6;
        
        Scene::EntityPtr entity = rexlogicmodule_->GetPrimEntity(localid);
        if(!entity) return;
        EC_NetworkPosition &netpos = *checked_static_cast<EC_NetworkPosition*>(entity->GetComponent(EC_NetworkPosition::NameStatic()).get());
        
        netpos.position_ = GetProcessedVector(&bytes[i]);
        i += sizeof(Core::Vector3df);
        
        netpos.velocity_ = GetProcessedScaledVectorFromUint16(&bytes[i],128);
        i += 6;
        
        netpos.accel_ = GetProcessedVectorFromUint16(&bytes[i]); 
        i += 6;

        netpos.rotation_ = GetProcessedQuaternion(&bytes[i]);
        i += 8;

        netpos.rotvel_ = GetProcessedScaledVectorFromUint16(&bytes[i],128);
        
        netpos.Updated();
    }
    
    bool Primitive::HandleRexGM_RexMediaUrl(OpenSimProtocol::NetworkEventInboundData* data)
    {
        /// \todo tucofixme
        return false;
    }    

    bool Primitive::HandleRexGM_RexPrimData(OpenSimProtocol::NetworkEventInboundData* data)
    {
        data->message->ResetReading();
        data->message->SkipToFirstVariableByName("Parameter");
        
        // Variable block begins
        size_t instance_count = data->message->ReadCurrentBlockInstanceCount();
        
        // First instance contains the UUID.
        RexUUID primuuid(data->message->ReadString());
                
        // Calculate full data size
        size_t fulldatasize = data->message->GetDataSize();
        size_t bytes_read = data->message->BytesRead();
        fulldatasize -= bytes_read;
        
        // Allocate memory block
        std::vector<Core::u8> fulldata;
        fulldata.resize(fulldatasize);
        int offset = 0;
        
        // Read the binary data.
        // The first instance contains always the UUID and rest of instances contain only binary data.
        // Data for multiple objects are never sent in the same message. All of the necessary data fits in one message.
        // Read the data:
        while(data->message->BytesRead() < data->message->GetDataSize())
        {
            const Core::u8* readbytedata = data->message->ReadBuffer(&bytes_read);
            memcpy(&fulldata[offset], readbytedata, bytes_read);
            offset += bytes_read;
        }

        Scene::EntityPtr entity = rexlogicmodule_->GetPrimEntity(primuuid);
        // If cannot get the entity, put to pending rexprimdata
        if (entity)           
            HandleRexPrimDataBlob(entity->GetId(), &fulldata[0]);
        else
            pending_rexprimdata_[primuuid] = fulldata;
        
        return false;
    }

    void Primitive::CheckPendingRexPrimData(Core::entity_id_t entityid)
    {
        Scene::EntityPtr entity = rexlogicmodule_->GetPrimEntity(entityid);
        if (!entity) return;
        EC_OpenSimPrim &prim = *checked_static_cast<EC_OpenSimPrim*>(entity->GetComponent(EC_OpenSimPrim::NameStatic()).get());
               
        RexPrimDataMap::iterator i = pending_rexprimdata_.find(prim.FullId);
        if (i != pending_rexprimdata_.end())
        {
            HandleRexPrimDataBlob(entityid, &i->second[0]);
            pending_rexprimdata_.erase(i);
        }
    }

    void Primitive::HandleRexPrimDataBlob(Core::entity_id_t entityid, const uint8_t* primdata)
    {
        int idx = 0;

        Scene::EntityPtr entity = rexlogicmodule_->GetPrimEntity(entityid);
        if (!entity) return;
        EC_OpenSimPrim &prim = *checked_static_cast<EC_OpenSimPrim*>(entity->GetComponent(EC_OpenSimPrim::NameStatic()).get());

        // graphical values
        prim.DrawType = ReadUInt8FromBytes(primdata,idx);
        prim.IsVisible = ReadBoolFromBytes(primdata,idx);
        prim.CastShadows = ReadBoolFromBytes(primdata,idx);
        prim.LightCreatesShadows = ReadBoolFromBytes(primdata,idx);
        prim.DescriptionTexture = ReadBoolFromBytes(primdata,idx);    
        prim.ScaleToPrim = ReadBoolFromBytes(primdata,idx);
        prim.DrawDistance = ReadFloatFromBytes(primdata,idx);
        prim.LOD = ReadFloatFromBytes(primdata,idx);

        prim.MeshUUID = ReadUUIDFromBytes(primdata,idx);
        prim.CollisionMesh = ReadUUIDFromBytes(primdata,idx);      
        
        prim.ParticleScriptUUID = ReadUUIDFromBytes(primdata,idx);

        // animation
        prim.AnimationPackageUUID = ReadUUIDFromBytes(primdata,idx);        
        prim.AnimationName = ReadNullTerminatedStringFromBytes(primdata,idx);
        prim.AnimationRate = ReadFloatFromBytes(primdata,idx);

        MaterialMap materials;
        uint8_t tempmaterialindex = 0; 
        uint8_t tempmaterialcount = ReadUInt8FromBytes(primdata,idx);
        for(int i=0;i<tempmaterialcount;i++)
        {
            MaterialData newmaterialdata;

            newmaterialdata.Type = ReadUInt8FromBytes(primdata,idx);
            newmaterialdata.UUID = ReadUUIDFromBytes(primdata,idx);
            tempmaterialindex = ReadUInt8FromBytes(primdata,idx);
            materials[tempmaterialindex] = newmaterialdata;                           
        }
        prim.Materials = materials;

        prim.ServerScriptClass = ReadNullTerminatedStringFromBytes(primdata,idx);
  
        // sound
        prim.SoundUUID = ReadUUIDFromBytes(primdata,idx);       
        prim.SoundVolume = ReadFloatFromBytes(primdata,idx);
        prim.SoundRadius = ReadFloatFromBytes(primdata,idx);               

        prim.SelectPriority = ReadUInt32FromBytes(primdata,idx);
        
        HandleDrawType(entityid);
    }
    
    bool Primitive::HandleOSNE_KillObject(uint32_t objectid)
    {
        Scene::ScenePtr scene = rexlogicmodule_->GetCurrentActiveScene();
        if (!scene)
            return false;

        RexTypes::RexUUID fullid;
        fullid.SetNull();
        Scene::EntityPtr entity = rexlogicmodule_->GetPrimEntity(objectid);
        if(!entity)
            return false;

        Foundation::ComponentPtr component = entity->GetComponent(EC_OpenSimPrim::NameStatic());
        if(component)
        {
            EC_OpenSimPrim &prim = *checked_static_cast<EC_OpenSimPrim*>(component.get());
            fullid = prim.FullId;
        }
        
        scene->RemoveEntity(objectid);
        rexlogicmodule_->UnregisterFullId(fullid);        
        return false;
    }    

    bool Primitive::HandleOSNE_ObjectProperties(OpenSimProtocol::NetworkEventInboundData* data)
    {
        NetInMessage *msg = data->message;
        msg->ResetReading();
        
        RexUUID full_id = msg->ReadUUID();
        msg->SkipToFirstVariableByName("Name");
        std::string name = msg->ReadString();
        std::string desc = msg->ReadString();
        ///\todo Handle rest of the vars.
        
        Scene::EntityPtr entity = rexlogicmodule_->GetPrimEntity(full_id);
        if(entity)
        {
            EC_OpenSimPrim &prim = *checked_static_cast<EC_OpenSimPrim*>(entity->GetComponent(EC_OpenSimPrim::NameStatic()).get());
            prim.ObjectName = name;
            prim.Description = desc;
            
            // Send the 'Entity Selected' event.
            Core::event_category_id_t event_category_id = rexlogicmodule_->GetFramework()->GetEventManager()->QueryEventCategory("Scene");
            Scene::Events::SceneEventData event_data(prim.LocalId);
            rexlogicmodule_->GetFramework()->GetEventManager()->SendEvent(event_category_id, Scene::Events::EVENT_ENTITY_SELECTED, &event_data);
        }
        else
            RexLogicModule::LogInfo("Received 'ObjectProperties' packet for unknown entity (" + full_id.ToString() + ").");
        
        return false;        
    }

    void Primitive::HandleDrawType(Core::entity_id_t entityid)
    {
        // Discard old request tags for this entity
        DiscardRequestTags(entityid, mesh_request_tags_);
        DiscardRequestTags(entityid, mesh_texture_request_tags_);
                                
        Scene::EntityPtr entity = rexlogicmodule_->GetPrimEntity(entityid);
        if (!entity) return;
        EC_OpenSimPrim &prim = *checked_static_cast<EC_OpenSimPrim*>(entity->GetComponent(EC_OpenSimPrim::NameStatic()).get());
            
        if ((prim.DrawType == RexTypes::DRAWTYPE_MESH) && (!prim.MeshUUID.IsNull()))
        {
            //RexLogicModule::LogInfo("Entity " + Core::ToString<Core::entity_id_t>(entityid) + 
            //    " has drawtype " + Core::ToString<int>(prim.DrawType) + " meshid " + prim.MeshUUID.ToString());
            
            // Get/create mesh component 
            Foundation::ComponentPtr mesh = entity->GetComponent(OgreRenderer::EC_OgreMesh::NameStatic());
            if (!mesh)
                entity->AddEntityComponent(mesh = rexlogicmodule_->GetFramework()->GetComponentManager()->CreateComponent(OgreRenderer::EC_OgreMesh::NameStatic()));
                            
            OgreRenderer::EC_OgreMesh* meshptr = checked_static_cast<OgreRenderer::EC_OgreMesh*>(mesh.get());
            
            // Attach to placeable if not yet attached
            if (!meshptr->GetPlaceable())
                meshptr->SetPlaceable(entity->GetComponent(OgreRenderer::EC_OgrePlaceable::NameStatic()));
                             
            // Change mesh if yet nonexistent/changed
            // assume name to be UUID of mesh asset, which should be true of OgreRenderer resources
            std::string mesh_name = prim.MeshUUID.ToString();
            if (meshptr->GetMeshName() != mesh_name)
            {
                boost::shared_ptr<OgreRenderer::Renderer> renderer = rexlogicmodule_->GetFramework()->GetServiceManager()->
                    GetService<OgreRenderer::Renderer>(Foundation::Service::ST_Renderer).lock();
                Core::request_tag_t tag = renderer->RequestResource(mesh_name, OgreRenderer::OgreMeshResource::GetTypeStatic());

                // Remember that we are going to get a resource event for this entity
                if (tag)
                    mesh_request_tags_[tag] = entityid;         
            }
                        
            // Handle scale mesh to prim-setting
            meshptr->SetScaleToUnity(prim.ScaleToPrim); 

            // Set adjustment orientation for mesh (a legacy haxor, Ogre meshes usually have Y-axis as vertical)
            Core::Quaternion adjust(Core::PI/2, 0, Core::PI);
            meshptr->SetAdjustOrientation(adjust);

            // Check/request mesh textures
            HandleMeshTextures(entityid);
        }
        else
        {
            // Remove mesh component if exists
            Foundation::ComponentPtr mesh = entity->GetComponent(OgreRenderer::EC_OgreMesh::NameStatic());
            if (mesh)
            {
                entity->RemoveEntityComponent(mesh = rexlogicmodule_->GetFramework()->GetComponentManager()->CreateComponent(OgreRenderer::EC_OgreMesh::NameStatic()));
            }
        }
    } 
    
    void Primitive::HandleMeshTextures(Core::entity_id_t entityid)
    {                        
        Scene::EntityPtr entity = rexlogicmodule_->GetPrimEntity(entityid);
        if (!entity) return;
        EC_OpenSimPrim &prim = *checked_static_cast<EC_OpenSimPrim*>(entity->GetComponent(EC_OpenSimPrim::NameStatic()).get());            
           
        Foundation::ComponentPtr mesh = entity->GetComponent(OgreRenderer::EC_OgreMesh::NameStatic());
        if (!mesh) return;
            
        OgreRenderer::EC_OgreMesh* meshptr = checked_static_cast<OgreRenderer::EC_OgreMesh*>(mesh.get());
     
        boost::shared_ptr<OgreRenderer::Renderer> renderer = rexlogicmodule_->GetFramework()->GetServiceManager()->
            GetService<OgreRenderer::Renderer>(Foundation::Service::ST_Renderer).lock();
                                        
        MaterialMap::const_iterator i = prim.Materials.begin();
        while (i != prim.Materials.end())
        {
            Core::uint idx = i->first;                
            // For now, handle only textures, not materials
            if ((i->second.Type == RexTypes::RexAT_Texture) && (!i->second.UUID.IsNull()))
            {
                std::string tex_name = i->second.UUID.ToString();

                //! \todo in the future material names will not correspond directly to texture names, so can't use this kind of check
                if (meshptr->GetMaterialName(idx) != tex_name)
                {
                    Foundation::ResourcePtr res = renderer->GetResource(tex_name, OgreRenderer::OgreTextureResource::GetTypeStatic());
                    if (res)
                        HandleMeshTextureReady(entityid, res);
                    else
                    {                
                        Core::request_tag_t tag = renderer->RequestResource(tex_name, OgreRenderer::OgreTextureResource::GetTypeStatic());

                        // Remember that we are going to get a resource event for this entity
                        if (tag)
                            mesh_texture_request_tags_[tag] = entityid;   
                    } 
                }
            }    
            ++i;
        }    
    }

    bool Primitive::HandleResourceEvent(Core::event_id_t event_id, Foundation::EventDataInterface* data)
    {
        if (event_id == Resource::Events::RESOURCE_READY)
        {
            Resource::Events::ResourceReady* event_data = checked_static_cast<Resource::Events::ResourceReady*>(data);
            Foundation::ResourcePtr res = event_data->resource_;            
            
            EntityResourceRequestMap::iterator i = mesh_request_tags_.find(event_data->tag_);
            if (i != mesh_request_tags_.end())
            {
                HandleMeshReady(i->second, res);
                mesh_request_tags_.erase(i);
                return true;
            }
            
            EntityResourceRequestMap::iterator j = mesh_texture_request_tags_.find(event_data->tag_);
            if (j != mesh_texture_request_tags_.end())
            {
                HandleMeshTextureReady(j->second, res);
                mesh_texture_request_tags_.erase(j);
                return true;
            }            
        }
        
        return false;
    }
    
    void Primitive::HandleMeshReady(Core::entity_id_t entityid, Foundation::ResourcePtr res)
    {     
        if (!res) return;
        if (res->GetType() != OgreRenderer::OgreMeshResource::GetTypeStatic()) return;
        
        Scene::EntityPtr entity = rexlogicmodule_->GetPrimEntity(entityid);
        if (!entity) return;
           
        Foundation::ComponentPtr mesh = entity->GetComponent(OgreRenderer::EC_OgreMesh::NameStatic());
        if (!mesh) return;
        OgreRenderer::EC_OgreMesh* meshptr = checked_static_cast<OgreRenderer::EC_OgreMesh*>(mesh.get());         
                                              
        meshptr->SetMesh(res->GetId());  
        // Check/set textures now that we have the mesh
        HandleMeshTextures(entityid);                    
    }

    void Primitive::HandleMeshTextureReady(Core::entity_id_t entityid, Foundation::ResourcePtr res)
    {
        if (!res) return;
        if (res->GetType() != OgreRenderer::OgreTextureResource::GetTypeStatic()) return;
               
        Scene::EntityPtr entity = rexlogicmodule_->GetPrimEntity(entityid);
        if (!entity) return;
        EC_OpenSimPrim &prim = *checked_static_cast<EC_OpenSimPrim*>(entity->GetComponent(EC_OpenSimPrim::NameStatic()).get());            
           
        Foundation::ComponentPtr mesh = entity->GetComponent(OgreRenderer::EC_OgreMesh::NameStatic());
        if (!mesh) return;
        OgreRenderer::EC_OgreMesh* meshptr = checked_static_cast<OgreRenderer::EC_OgreMesh*>(mesh.get());      
        // If don't have the actual mesh entity yet, no use trying to set texture
        if (!meshptr->GetEntity()) return;     
                        
        MaterialMap::const_iterator i = prim.Materials.begin();
        while (i != prim.Materials.end())
        {
            Core::uint idx = i->first;                
            // For now, handle only textures, not materials
            if ((i->second.Type == RexTypes::RexAT_Texture) && (i->second.UUID.ToString() == res->GetId()))
            {
                // debug material creation to see diffuse textures                        
                Ogre::MaterialPtr mat = OgreRenderer::GetOrCreateUnlitTexturedMaterial(res->GetId().c_str());
                OgreRenderer::SetTextureUnitOnMaterial(mat, 0, res->GetId().c_str());
               
                meshptr->SetMaterial(idx, res->GetId());
            }
            ++i;
        }
    }
       
    void Primitive::DiscardRequestTags(Core::entity_id_t entityid, EntityResourceRequestMap& map)
    {
        Core::RequestTagVector tags_to_remove;
        EntityResourceRequestMap::iterator i = map.begin();
        while (i != map.end())
        {
            if (i->second == entityid)
                tags_to_remove.push_back(i->first);
            ++i;
        }
        for (int j = 0; j < tags_to_remove.size(); ++j)
            map.erase(tags_to_remove[j]);
    }                
}