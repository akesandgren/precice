// Copyright (C) 2011 Technische Universitaet Muenchen
// This file is part of the preCICE project. For conditions of distribution and
// use, please see the license notice at http://www5.in.tum.de/wiki/index.php/PreCICE_License
#include "ParticipantConfiguration.hpp"
#include "precice/impl/MeshContext.hpp"
#include "precice/impl/DataContext.hpp"
#include "precice/impl/WatchPoint.hpp"
#include "com/config/CommunicationConfiguration.hpp"
#include "action/config/ActionConfiguration.hpp"
#include "mesh/config/MeshConfiguration.hpp"
#include "geometry/config/GeometryConfiguration.hpp"
#include "spacetree/config/SpacetreeConfiguration.hpp"
#include "mapping/Mapping.hpp"
#include "mapping/config/MappingConfiguration.hpp"
#include "utils/Globals.hpp"
#include "utils/xml/XMLAttribute.hpp"
#include "utils/xml/ValidatorEquals.hpp"
#include "utils/xml/ValidatorOr.hpp"
#include "utils/Dimensions.hpp"

namespace precice {
namespace config {

tarch::logging::Log ParticipantConfiguration::
    _log("precice::config::ParticipantConfiguration");

//const std::string& ParticipantConfiguration:: getTag()
//{
//  static std::string tag("participant");
//  return tag;
//}

ParticipantConfiguration:: ParticipantConfiguration
(
  utils::XMLTag&                              parent,
  const mesh::PtrMeshConfiguration&           meshConfiguration,
  const geometry::PtrGeometryConfiguration&   geometryConfiguration,
  const spacetree::PtrSpacetreeConfiguration& spacetreeConfiguration )
:
  TAG("participant"),
  TAG_WRITE("write-data"),
  TAG_READ("read-data"),
  TAG_DATA_ACTION("data-action"),
  TAG_USE_MESH("use-mesh"),
  TAG_WATCH_POINT("watch-point"),
  TAG_SERVER("server"),
  ATTR_NAME("name"),
  ATTR_SOURCE_DATA("source-data"),
  ATTR_TARGET_DATA("target-data"),
  ATTR_TIMING("timing"),
  ATTR_LOCAL_OFFSET("offset"),
  ATTR_ACTION_TYPE("type"),
  ATTR_FROM("from"),
  ATTR_PROVIDE("provide"),
  ATTR_MESH("mesh"),
  ATTR_COORDINATE("coordinate"),
  ATTR_COMMUNICATION("communication"),
  ATTR_CONTEXT("context"),
  _dimensions(0),
  _meshConfig(meshConfiguration),
  _geometryConfig(geometryConfiguration),
  _spacetreeConfig(spacetreeConfiguration),
  _mappingConfig(),
  _actionConfig(),
  //_isValid(false),
  _participants(),
  _watchPointConfigs()
{
  //assertion1 ( (_dimensions == 2) || (_dimensions == 3), _dimensions );
  assertion(_meshConfig.use_count() > 0);
  using namespace utils;
  std::string doc;
  XMLTag tag(*this, TAG, XMLTag::OCCUR_ONCE_OR_MORE);
  doc = "Represents one solver using preCICE. In a coupled simulation, two ";
  doc += "participants have to be defined, while in geometry mode (see tag ";
  doc += "<solver-interface>) only one participant is necessary.";
  tag.setDocumentation(doc);

  XMLAttribute<std::string> attrName(ATTR_NAME);
  doc = "Name of the participant. Has to match the name given on construction ";
  doc += "of the precice::SolverInterface object used by the participant.";
  attrName.setDocumentation(doc);
  tag.addAttribute(attrName);

  XMLTag tagWriteData(*this, TAG_WRITE, XMLTag::OCCUR_ARBITRARY);
  doc = "Sets data to be written by the participant to preCICE. ";
  doc += "Data is defined by using the <data> tag.";
  tagWriteData.setDocumentation(doc);
  XMLTag tagReadData(*this, TAG_READ, XMLTag::OCCUR_ARBITRARY);
  doc = "Sets data to be read by the participant from preCICE. ";
  doc += "Data is defined by using the <data> tag.";
  tagReadData.setDocumentation(doc);
  XMLAttribute<std::string> attrDataName(ATTR_NAME);
  doc = "Name of the data.";
  attrDataName.setDocumentation(doc);
  tagWriteData.addAttribute(attrDataName);
  tagReadData.addAttribute(attrDataName);
  XMLAttribute<std::string> attrMesh(ATTR_MESH);
  doc = "Mesh the data belongs to. If data should be read/written to several ";
  doc += "meshes, this has to be specified separately for each mesh.";
  attrMesh.setDocumentation(doc);
  tagWriteData.addAttribute(attrMesh);
  tagReadData.addAttribute(attrMesh);
  tag.addSubtag(tagWriteData);
  tag.addSubtag(tagReadData);

  _mappingConfig = mapping::PtrMappingConfiguration(
                   new mapping::MappingConfiguration(tag, _meshConfig));

  _actionConfig = action::PtrActionConfiguration(
                  new action::ActionConfiguration(tag, _meshConfig));

  _exportConfig = io::PtrExportConfiguration(new io::ExportConfiguration(tag));

  XMLTag tagWatchPoint(*this, TAG_WATCH_POINT, XMLTag::OCCUR_ARBITRARY);
  doc = "A watch point can be used to follow the transient changes of data ";
  doc += "and mesh vertex coordinates at a given point";
  tagWatchPoint.setDocumentation(doc);
  doc = "Name of the watch point. Is taken in combination with the participant ";
  doc += "name to construct the filename the watch point data is written to.";
  attrName.setDocumentation(doc);
  tagWatchPoint.addAttribute(attrName);
  doc = "Mesh to be watched.";
  attrMesh.setDocumentation(doc);
  tagWatchPoint.addAttribute(attrMesh);
  XMLAttribute<utils::DynVector> attrCoordinate(ATTR_COORDINATE);
  doc = "The coordinates of the watch point. I the watch point is not put exactly ";
  doc += "on the mesh to observe, the closest projection of the point onto the ";
  doc += "mesh is considered instead, and values/coordinates are interpolated ";
  doc += "linearly to that point.";
  attrCoordinate.setDocumentation(doc);
  tagWatchPoint.addAttribute(attrCoordinate);
  tag.addSubtag(tagWatchPoint);

  XMLTag tagUseMesh(*this, TAG_USE_MESH, XMLTag::OCCUR_ARBITRARY);
  doc = "Makes a mesh (see tag <mesh> available to a participant.";
  tagUseMesh.setDocumentation(doc);
  attrName.setDocumentation("Name of the mesh.");
  tagUseMesh.addAttribute(attrName);
  XMLAttribute<utils::DynVector> attrLocalOffset(ATTR_LOCAL_OFFSET);
  doc = "The mesh can have an offset only applied for the local participant.";
  attrLocalOffset.setDocumentation(doc);
  attrLocalOffset.setDefaultValue(utils::DynVector(3, 0.0));
  tagUseMesh.addAttribute(attrLocalOffset);

  XMLAttribute<std::string> attrFrom(ATTR_FROM);
  doc = "A mesh might not be constructed by a geometry (see tags <geometry:...>), ";
  doc += "but by a solver directly. If a solver created mesh should be used by ";
  doc += "another solver, this attribute has to specify the creating participant's";
  doc += " name. The creator has to use the attribute \"provide\" to signal he is ";
  doc += "providing the mesh geometry.";
  attrFrom.setDocumentation(doc);
  attrFrom.setDefaultValue("");
  tagUseMesh.addAttribute(attrFrom);
  XMLAttribute<bool> attrProvide(ATTR_PROVIDE);
  doc = "A mesh might not be constructed by a geometry (see tags<geometry:...>), ";
  doc += "but by a solver directly. If this attribute is set to \"on\", the ";
  doc += "participant has to create the mesh geometry before initializing preCICE.";
  attrProvide.setDocumentation(doc);
  attrProvide.setDefaultValue(false);
  tagUseMesh.addAttribute(attrProvide);
  tag.addSubtag(tagUseMesh);

  std::list<XMLTag> serverTags;
  XMLTag::Occurrence serverOcc = XMLTag::OCCUR_NOT_OR_ONCE;
  {
    XMLTag tagServer(*this, "sockets", serverOcc, TAG_SERVER);
    doc = "When a solver runs in parallel, it has to use preCICE in form of a ";
    doc += "separatly running server. This is enabled by this tag. ";
    doc += "The communication between participant and server is done by sockets.";
    tagServer.setDocumentation(doc);

    XMLAttribute<int> attrPort("port");
    doc = "Port number to be used by server for socket communiation.";
    attrPort.setDocumentation(doc);
    attrPort.setDefaultValue(51235);
    tagServer.addAttribute(attrPort);

    serverTags.push_back(tagServer);
  }
  {
    XMLTag tagServer(*this, "mpi", serverOcc, TAG_SERVER);
    doc = "When a solver runs in parallel, it has to use preCICE in form of a ";
    doc += "separatly running server. This is enabled by this tag. ";
    doc += "The communication between participant and server is done by mpi ";
    doc += "with startup in separated communication spaces.";
    tagServer.setDocumentation(doc);

    XMLAttribute<std::string> attrExchangeDirectory("exchange-directory");
    doc = "Directory where connection information is exchanged. By default, the ";
    doc += "directory of startup is chosen, and both solvers have to be started ";
    doc += "in the same directory.";
    attrExchangeDirectory.setDocumentation(doc);
    attrExchangeDirectory.setDefaultValue("");
    tagServer.addAttribute(attrExchangeDirectory);

    serverTags.push_back(tagServer);
  }
  {
    XMLTag tagServer(*this, "mpi-single", serverOcc, TAG_SERVER);
    doc = "When a solver runs in parallel, it has to use preCICE in form of a ";
    doc += "separatly running server. This is enabled by this tag. ";
    doc += "The communication between participant and server is done by mpi ";
    doc += "with startup in a common communication space.";
    tagServer.setDocumentation(doc);
    serverTags.push_back(tagServer);
  }
  foreach (XMLTag& tagServer, serverTags){
    tag.addSubtag(tagServer);
  }

//  XMLAttribute<std::string> attrCom(ATTR_COMMUNICATION);
//  doc = "Sets the communication means to transmit data between solver processes ";
//  doc += "and preCICE server (see tag <communication>).";
//  ValidatorEquals<std::string> validMPI("mpi");
//  ValidatorEquals<std::string> validMPISingle("mpi-single");
//  ValidatorEquals<std::string> validSockets("sockets");
//  attrCom.setValidator(validMPI || validMPISingle || validSockets);
//  attrCom.setDocumentation(doc);
//  tagServer.addAttribute(attrCom);
//  XMLAttribute<std::string> attrContext(ATTR_CONTEXT);
//  doc = "Sets the communication context (see tag <communication>).";
//  attrContext.setDocumentation(doc);
//  attrContext.setDefaultValue("");
//  tagServer.addAttribute(attrContext);
//  tag.addSubtag(tagServer);

  parent.addSubtag(tag);
}

void ParticipantConfiguration:: setDimensions
(
  int dimensions )
{
  preciceTrace1("setDimensions()", dimensions);
  assertion1((dimensions == 2) || (dimensions == 3), dimensions);
  _dimensions = dimensions;
}

//bool ParticipantConfiguration:: parseSubtag
//(
//  utils::XMLTag::XMLReader* xmlReader )
//{
//  using utils::XMLTag;
//  using utils::XMLAttribute;
//  using utils::ValidatorEquals;
//  XMLTag tagParticipant ( TAG, XMLTag::OCCUR_ONCE );
//
//  XMLAttribute<std::string> attrName ( ATTR_NAME );
//  tagParticipant.addAttribute ( attrName );
//
//  XMLTag tagWriteData ( TAG_WRITE, XMLTag::OCCUR_ARBITRARY);
//  XMLTag tagReadData  ( TAG_READ, XMLTag::OCCUR_ARBITRARY);
//  XMLAttribute<std::string> attrDataName ( ATTR_NAME );
//  tagWriteData.addAttribute ( attrDataName );
//  tagReadData.addAttribute ( attrDataName );
//  XMLAttribute<std::string> attrMesh ( ATTR_MESH );
//  tagWriteData.addAttribute ( attrMesh );
//  tagReadData.addAttribute ( attrMesh );
//  tagParticipant.addSubtag ( tagWriteData );
//  tagParticipant.addSubtag ( tagReadData );
//
//  XMLTag tagMapping ( mapping::MappingConfiguration::TAG, XMLTag::OCCUR_ARBITRARY );
//  tagParticipant.addSubtag ( tagMapping );
//
//  XMLTag tagAction ( action::ActionConfiguration::TAG, XMLTag::OCCUR_ARBITRARY );
//  tagParticipant.addSubtag ( tagAction );
//
//  XMLTag tagExport ( io::ExportConfiguration::TAG, XMLTag::OCCUR_ARBITRARY );
//  tagParticipant.addSubtag ( tagExport );
//
//  XMLTag tagWatchPoint ( TAG_WATCH_POINT, XMLTag::OCCUR_ARBITRARY );
//  tagWatchPoint.addAttribute ( attrName );
//  XMLAttribute<std::string> attrGeometry ( ATTR_MESH );
//  tagWatchPoint.addAttribute ( attrGeometry );
//  if (_dimensions == 2){
//    XMLAttribute<utils::Vector2D> attrCoordinate ( ATTR_COORDINATE );
//    tagWatchPoint.addAttribute ( attrCoordinate );
//  }
//  else {
//    XMLAttribute<utils::Vector3D> attrCoordinate ( ATTR_COORDINATE );
//    tagWatchPoint.addAttribute ( attrCoordinate );
//  }
//  tagParticipant.addSubtag ( tagWatchPoint );
//
//  XMLTag tagUseMesh ( TAG_USE_MESH, XMLTag::OCCUR_ARBITRARY );
//  tagUseMesh.addAttribute ( attrName );
//  if ( _dimensions == 2 ){
//    XMLAttribute<utils::Vector2D> attrLocalOffset ( ATTR_LOCAL_OFFSET );
//    attrLocalOffset.setDefaultValue ( utils::Vector2D(0.0) );
//    tagUseMesh.addAttribute ( attrLocalOffset );
//  }
//  else {
//    XMLAttribute<utils::Vector3D> attrLocalOffset ( ATTR_LOCAL_OFFSET );
//    attrLocalOffset.setDefaultValue ( utils::Vector3D(0.0) );
//    tagUseMesh.addAttribute ( attrLocalOffset );
//  }
//  XMLAttribute<std::string> attrFrom ( ATTR_FROM );
//  attrFrom.setDefaultValue ( "" );
//  tagUseMesh.addAttribute ( attrFrom );
//  XMLAttribute<bool> attrProvide ( ATTR_PROVIDE );
//  attrProvide.setDefaultValue ( false );
//  tagUseMesh.addAttribute ( attrProvide );
//  tagParticipant.addSubtag ( tagUseMesh );
//
//  XMLTag tagServer ( TAG_SERVER, XMLTag::OCCUR_NOT_OR_ONCE );
//  XMLAttribute<std::string> attrCom ( ATTR_COMMUNICATION );
//  tagServer.addAttribute ( attrCom );
//  XMLAttribute<std::string> attrContext(ATTR_CONTEXT);
//  attrContext.setDefaultValue("");
//  tagServer.addAttribute(attrContext);
//  tagParticipant.addSubtag ( tagServer );
//
//  _isValid = tagParticipant.parse ( xmlReader, *this );
//  return _isValid;
//}

void ParticipantConfiguration:: xmlTagCallback
(
  utils::XMLTag& tag )
{
  preciceTrace1 ( "xmlTagCallback()", tag.getName() );
  if (tag.getName() == TAG){
    std::string name = tag.getStringAttributeValue(ATTR_NAME);
    impl::PtrParticipant p(new impl::Participant(name, _meshConfig));
    _participants.push_back(p);
  }
  else if (tag.getName() == TAG_USE_MESH){
    assertion(_dimensions != 0); // setDimensions() has been called
    std::string name = tag.getStringAttributeValue(ATTR_NAME);
    utils::DynVector offset(_dimensions);
    offset = tag.getDynVectorAttributeValue(ATTR_LOCAL_OFFSET, _dimensions);
    std::string from = tag.getStringAttributeValue(ATTR_FROM);
    bool provide = tag.getBooleanAttributeValue(ATTR_PROVIDE);
    mesh::PtrMesh mesh = _meshConfig->getMesh(name);
    if (mesh.get() == 0){
      std::ostringstream stream;
      stream << "Participant \"" << _participants.back()->getName()
             << "\" uses mesh \"" << name << "\" which is not defined";
      throw stream.str();
    }
    geometry::PtrGeometry geo ( _geometryConfig->getGeometry(name) );
    spacetree::PtrSpacetree spacetree;
    if (_meshConfig->doesMeshUseSpacetree(name)){
      std::string spacetreeName = _meshConfig->getSpacetreeName(name);
      spacetree = _spacetreeConfig->getSpacetree ( spacetreeName );
    }
    _participants.back()->useMesh ( mesh, geo, spacetree, offset, false, from, provide );
  }
//  else if ( tag.getName() == mapping::MappingConfiguration::TAG ) {
//    return _mappingConfig->parseSubtag ( xmlReader );
//  }
  else if ( tag.getName() == TAG_WRITE ) {
    std::string dataName = tag.getStringAttributeValue(ATTR_NAME);
    std::string meshName = tag.getStringAttributeValue(ATTR_MESH);
    mesh::PtrMesh mesh = _meshConfig->getMesh ( meshName );
    preciceCheck ( mesh.use_count() > 0, "xmlTagCallback()", "Participant "
                   << "\"" << _participants.back()->getName() << "\" has to use "
                   << "mesh \"" << meshName << "\" in order to write data to it!" );
    mesh::PtrData data = getData ( mesh, dataName );
    _participants.back()->addWriteData ( data, mesh );
  }
  else if ( tag.getName() == TAG_READ ) {
    std::string dataName = tag.getStringAttributeValue(ATTR_NAME);
    std::string meshName = tag.getStringAttributeValue(ATTR_MESH);
    mesh::PtrMesh mesh = _meshConfig->getMesh ( meshName );
    preciceCheck ( mesh.use_count() > 0, "xmlTagCallback()", "Participant "
                   << "\"" << _participants.back()->getName() << "\" has to use "
                   << "mesh \"" << meshName << "\" in order to read data from it!" );
    mesh::PtrData data = getData ( mesh, dataName );
    _participants.back()->addReadData ( data, mesh );
  }
//  else if ( tag.getName() == action::ActionConfiguration::TAG ){
//    action::ActionConfiguration config ( _meshConfig );
//    bool success = config.parseSubtag ( xmlReader );
//    if ( success ){
//      bool used = _participants.back()->isMeshUsed( config.getUsedMeshID() );
//      preciceCheck ( used, "xmlTagCallback()", "Data action of participant "
//                     << _participants.back()->getName()
//                     << "\" uses mesh which is not used by the participant!" );
//      _participants.back()->addAction ( config.getAction() );
//    }
//    return success;
//  }
//  else if ( tag.getName() == io::ExportConfiguration::TAG ){
//    io::ExportConfiguration config;
//    if ( config.parseSubtag(xmlReader) ){
//      //preciceDebug ( "Setting export context with "
//      //               << config.exports().size() << " exports" );
//      _participants.back()->addExportContext ( config.getExportContext() );
//      return true;
//    }
//    return false;
//  }
  else if ( tag.getName() == TAG_WATCH_POINT ){
    assertion(_dimensions != 0); // setDimensions() has been called
    WatchPointConfig config;
    config.name = tag.getStringAttributeValue(ATTR_NAME);
    config.nameMesh = tag.getStringAttributeValue(ATTR_MESH);
    config.coordinates.append(
        tag.getDynVectorAttributeValue(ATTR_COORDINATE, _dimensions));
    _watchPointConfigs.push_back(config);
  }
  else if (tag.getNamespace() == TAG_SERVER){
    //std::string comType = tag.getStringAttributeValue(ATTR_COMMUNICATION);
    //std::string comContext = tag.getStringAttributeValue(ATTR_CONTEXT);
    com::CommunicationConfiguration comConfig;
    com::PtrCommunication com = comConfig.createCommunication(tag);
    _participants.back()->setClientServerCommunication(com);
  }
}

void ParticipantConfiguration:: xmlEndTagCallback
(
  utils::XMLTag& tag )
{
  if (tag.getName() == TAG){
    finishParticipantConfiguration(_participants.back());
  }
//  else if (tag.getName() == action::ActionConfiguration::TAG){
//    bool used = _participants.back()->isMeshUsed(_actionConfig->getUsedMeshID());
//    preciceCheck(used, "xmlTagCallback()", "Data action of participant "
//                 << _participants.back()->getName()
//                 << "\" uses mesh which is not used by the participant!");
//    _participants.back()->addAction(_actionConfig->getAction());
//  }
}


//bool ParticipantConfiguration:: isValid() const
//{
//  return _isValid;
//}

void ParticipantConfiguration:: addParticipant
(
  const impl::PtrParticipant&             participant,
  const mapping::PtrMappingConfiguration& mappingConfig )
{
  _participants.push_back ( participant );
  _mappingConfig = mappingConfig;
  finishParticipantConfiguration ( participant );
}

const std::vector<impl::PtrParticipant>&
ParticipantConfiguration:: getParticipants() const
{
  return _participants;
}

mesh::PtrMesh ParticipantConfiguration:: copy
(
  const mesh::PtrMesh& mesh ) const
{
  int dim = mesh->getDimensions();
  std::string name(mesh->getName());
  bool flipNormals = mesh->isFlipNormals();
  mesh::Mesh* meshCopy = new mesh::Mesh("Local_" + name, dim, flipNormals);
  foreach (const mesh::PtrData& data, mesh->data()){
    meshCopy->createData(data->getName(), data->getDimensions());
  }
  return mesh::PtrMesh(meshCopy);
}

const mesh::PtrData & ParticipantConfiguration:: getData
(
  const mesh::PtrMesh& mesh,
  const std::string&   nameData ) const
{
  foreach ( const mesh::PtrData & data, mesh->data() ){
    if ( data->getName() == nameData ) {
      return data;
    }
  }
  preciceError ( "getData()", "Participant \"" << _participants.back()->getName()
                 << "\" assignes data \"" << nameData << "\" wrongly to mesh \""
                 << mesh->getName() << "\"!" );
}

void ParticipantConfiguration:: finishParticipantConfiguration
(
  const impl::PtrParticipant& participant )
{
  preciceTrace1("finishParticipantConfiguration()", participant->getName());

  // Set input/output meshes for data mappings and mesh requirements
  typedef mapping::MappingConfiguration::ConfiguredMapping ConfMapping;
  foreach (const ConfMapping& confMapping, _mappingConfig->mappings()){
    int meshID = confMapping.mesh->getID();
    preciceCheck(participant->isMeshUsed(meshID), "finishParticipantConfiguration()",
        "Participant \"" << participant->getName() << "\" has mapping"
        << " to/from mesh \"" << confMapping.mesh->getName() << "\" which he does not use!");
    impl::MeshContext& meshContext = participant->meshContext(meshID);

    if (confMapping.direction == mapping::MappingConfiguration::WRITE){
      mapping::PtrMapping& map = meshContext.writeMappingContext.mapping;
      assertion(map.get() == NULL);
      map = confMapping.mapping;
      if (map->getInputRequirement() > meshContext.meshRequirement){
        meshContext.meshRequirement = map->getInputRequirement();
      }
      if (meshContext.writeMappingContext.localMesh.get() == NULL){
        meshContext.writeMappingContext.localMesh = copy(meshContext.mesh);
      }
      const mesh::PtrMesh& input = meshContext.writeMappingContext.localMesh;
      const mesh::PtrMesh& output = meshContext.mesh;
      preciceDebug("Configure write mapping for input=" << input->getName()
                   << ", output=" << output->getName());
      map->setMeshes(input, output);
      meshContext.writeMappingContext.timing = confMapping.timing;
      //meshContext.writeMappingContext.isIncremental = confMapping.isIncremental;
    }
    else {
      assertion(confMapping.direction == mapping::MappingConfiguration::READ);
      mapping::PtrMapping& map = meshContext.readMappingContext.mapping;
      assertion(map.get() == NULL);
      map = confMapping.mapping;
      if (map->getOutputRequirement() > meshContext.meshRequirement){
        meshContext.meshRequirement = map->getOutputRequirement();
      }
      if (meshContext.readMappingContext.localMesh.get() == NULL){
        meshContext.readMappingContext.localMesh = copy(meshContext.mesh);
      }
      const mesh::PtrMesh& input = meshContext.mesh;
      const mesh::PtrMesh& output = meshContext.readMappingContext.localMesh;
      preciceDebug("Configure read mapping for input=" << input->getName()
                   << ", output=" << output->getName());
      map->setMeshes(input, output);
      meshContext.readMappingContext.timing = confMapping.timing;
      //meshContext.readMappingContext.isIncremental = confMapping.isIncremental;
    }
    if (confMapping.timing == mapping::MappingConfiguration::INITIAL
        || (confMapping.timing != mapping::MappingConfiguration::INCREMENTAL))
    {
      if ( meshContext.meshRequirement == mapping::Mapping::TEMPORARY ){
        meshContext.meshRequirement = mapping::Mapping::VERTEX;
      }
    }
  }
  _mappingConfig->resetMappings();

  // Set participant data for data contexts
  foreach (impl::DataContext& dataContext, participant->writeDataContexts()){
    int meshID = dataContext.mesh->getID();
    preciceCheck(participant->isMeshUsed(meshID), "finishParticipant()",
        "Participant \"" << participant->getName() << "\" has to use mesh \""
        << dataContext.mesh->getName() << "\" when writing data to it!");
    impl::MeshContext& meshContext = participant->meshContext(meshID);
    if (meshContext.writeMappingContext.mapping.get() != NULL){
      dataContext.mappingContext = meshContext.writeMappingContext;
      setLocalData(dataContext, meshContext.mesh);
    }
  }
  foreach (impl::DataContext& dataContext, participant->readDataContexts()){
    int meshID = dataContext.mesh->getID();
    preciceCheck(participant->isMeshUsed(meshID), "finishParticipant()",
                 "Participant \"" << participant->getName()
                 << "\" has to use mesh \"" << dataContext.mesh->getName()
                 << "\" when reading data from it!");
    impl::MeshContext & meshContext = participant->meshContext(meshID);
    if (meshContext.readMappingContext.mapping.get() != NULL){
      dataContext.mappingContext = meshContext.readMappingContext;
      setLocalData(dataContext, meshContext.mesh);
    }
  }

  // Add actions
  foreach (const action::PtrAction& action, _actionConfig->actions()){
    bool used = _participants.back()->isMeshUsed(action->getMesh()->getID());
    preciceCheck(used, "xmlTagCallback()", "Data action of participant "
                 << _participants.back()->getName()
                 << "\" uses mesh which is not used by the participant!");
    _participants.back()->addAction(action);
  }
  _actionConfig->resetActions();

  // Add export contexts
  foreach (const io::ExportContext& context, _exportConfig->exportContexts()){
    _participants.back()->addExportContext(context);
  }
  _exportConfig->resetExports();

  // Create watch points
  foreach ( const WatchPointConfig & config, _watchPointConfigs ){
    mesh::PtrMesh mesh;
    foreach ( const impl::MeshContext & context, participant->usedMeshContexts() ){
      if ( context.mesh->getName() == config.nameMesh ){
        mesh = context.mesh;
      }
    }
    preciceCheck ( mesh.use_count() > 0, "xmlEndTagCallback()",
                   "Participant \"" << participant->getName()
                   << "\" defines watchpoint \"" << config.name
                   << "\" for mesh \"" << config.nameMesh
                   << "\" which is not used by him!" );
    std::string filename = config.name + ".watchpoint.txt";
    impl::PtrWatchPoint watchPoint (
        new impl::WatchPoint(config.coordinates, mesh, filename) );
    participant->addWatchPoint ( watchPoint );
  }
  _watchPointConfigs.clear ();
}

void ParticipantConfiguration:: setLocalData
(
  impl::DataContext&   dataContext,
  const mesh::PtrMesh& mesh )
{
  preciceTrace2("setLocalData()", dataContext.data->getName(), mesh->getName());
  int dataID = dataContext.data->getID();
  std::string dataName = mesh->data(dataID)->getName();
  const mesh::PtrMesh& localMesh = dataContext.mappingContext.localMesh;
  foreach (const mesh::PtrData& data, localMesh->data()){
    if (data->getName() == dataName){
      dataContext.localData = data;
      break;
    }
  }
  assertion(dataContext.localData.get() != NULL);
}

}} // namespace precice, config