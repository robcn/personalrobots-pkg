#include <cstdio>
#include <cstdlib>
#include <cmath>

#include <vector>
#include <string>
#include <sstream>

#include <urdf/URDF.h>
#include <libTF/Pose3D.h>

std::string values2str(unsigned int count, const double *values, double (*conv)(double) = NULL)
{
    std::stringstream ss;
    for (unsigned int i = 0 ; i < count ; i++)
    {
	if (i > 0)
	    ss << " ";
	ss << (conv ? conv(values[i]) : values[i]);
    }
    return ss.str();
}

double rad2deg(double v)
{
    return v * 180.0 / M_PI;
}

void setupTransform(libTF::Pose3D &transform, const double *xyz, const double *rpy)
{
    transform.setFromEuler(xyz[0], xyz[1], xyz[2], rpy[2], rpy[1], rpy[0]);
}

void addKeyValue(TiXmlElement *elem, const std::string& key, const std::string &value)
{
    TiXmlElement *ekey      = new TiXmlElement(key);
    TiXmlText    *text_ekey = new TiXmlText(value);
    ekey->LinkEndChild(text_ekey);    
    elem->LinkEndChild(ekey); 
}

void addTransform(TiXmlElement *elem, const::libTF::Pose3D& transform)
{
    libTF::Pose3D::Position pz;
    transform.getPosition(pz);
    double cpos[3] = { pz.x, pz.y, pz.z };
    
    libTF::Pose3D::Euler eu;
    transform.getEuler(eu);
    double crot[3] = { eu.roll, eu.pitch, eu.yaw };		    
    
    /* set geometry transform */
    addKeyValue(elem, "xyz", values2str(3, cpos));
    addKeyValue(elem, "rpy", values2str(3, crot, rad2deg));  
}

void convertLink(TiXmlElement *root, robot_desc::URDF::Link *link, const libTF::Pose3D &transform)
{
    libTF::Pose3D currentTransform = transform;
    
    std::string type;
    unsigned int nsize = 0;

    switch (link->collision->geometry->type)
    {
    case robot_desc::URDF::Link::Geometry::BOX:
	nsize = 3;
	type = "box";
	break;
    case robot_desc::URDF::Link::Geometry::CYLINDER:
	nsize = 2;
	type = "cylinder";
	break;
    case robot_desc::URDF::Link::Geometry::SPHERE:
	nsize = 1;
	type = "sphere";
	break;
    default:
	nsize = 0;
	printf("Unknown body type: %d in link '%s'\n", link->collision->geometry->type, link->name.c_str());
	break;
    }
    
    if (!type.empty())
    {
	/* create new body */
	TiXmlElement *elem     = new TiXmlElement("body:" + type);
	
	/* set body name */
	elem->SetAttribute("name", link->name);
	
	/* compute global transform */
	libTF::Pose3D localTransform;
	setupTransform(localTransform, link->xyz, link->rpy);
	currentTransform.multiplyPose(localTransform);
	addTransform(elem, currentTransform);
	
	/* create geometry node */
	TiXmlElement *geom     = new TiXmlElement("geom:" + type);
	
	{		
	    /* set its name */
	    geom->SetAttribute("name", link->collision->geometry->name);
	    
	    /* set transform */
	    addKeyValue(geom, "xyz", values2str(3, link->collision->xyz));
	    addKeyValue(geom, "rpy", values2str(3, link->collision->rpy, rad2deg));
	    
	    /* set mass properties */
	    addKeyValue(geom, "massMatrix", "true");
	    addKeyValue(geom, "mass", values2str(1, &link->inertial->mass));
	    static const char tagList1[6][4] = {"ixx", "ixy", "ixz", "iyy", "iyz", "izz"};
	    for (int j = 0 ; j < 6 ; ++j)
		addKeyValue(geom, tagList1[j], values2str(1, link->inertial->inertia + j));
	    static const char tagList2[3][3] = {"cx", "cy", "cz"};
	    for (int j = 0 ; j < 3 ; ++j)
		addKeyValue(geom, tagList2[j], values2str(1, link->inertial->com + j));
	    
	    /* set additional data */
	    static const char tagList3[4][4] = {"kp", "kd", "mu1", "mu2"};
	    for (int j = 0 ; j < 4 ; ++j)
		if (link->collision->data.hasDefaultValue(tagList3[j]))
		    addKeyValue(geom, tagList3[j], link->collision->data.getDefaultValue(tagList3[j]));
	    
	    /* set geometry size */
	    addKeyValue(geom, "size", values2str(nsize, link->collision->geometry->size));
	    
	    /* create visual node */
	    TiXmlElement *visual = new TiXmlElement("visual");
	    {
		/* compute the visualisation transfrom */
		libTF::Pose3D coll;
		setupTransform(coll, link->collision->xyz, link->collision->rpy);
		coll.invert();
		
		libTF::Pose3D vis;
		setupTransform(vis, link->visual->xyz, link->visual->rpy);
		coll.multiplyPose(vis);
		
		addTransform(visual, coll);
		
		/* set geometry scale */
		addKeyValue(visual, "scale", values2str(3, link->visual->scale));
		
		/* set geometry mesh file */
		if (link->visual->geometry->filename.empty())
		    addKeyValue(visual, "mesh", "unit_" + type);
		else
		    addKeyValue(visual, "mesh", link->visual->geometry->filename);
		
		/* set geometry material */		    
		if (!link->visual->material.empty())
		    addKeyValue(visual, "material", link->visual->material);
	    }
	    
	    geom->LinkEndChild(visual);
	}
	
	/* add geometry to body */
	elem->LinkEndChild(geom);	    
	
	/* add body to document */
	root->LinkEndChild(elem);
	
	/* compute the joint tag */
	std::string jtype;
	switch (link->joint->type)
	{
	case robot_desc::URDF::Link::Joint::REVOLUTE:
	    jtype = "hinge";
	    break;
	case robot_desc::URDF::Link::Joint::PRISMATIC:
	    jtype = "slider";
	    break;
	case robot_desc::URDF::Link::Joint::FLOATING:
	    break;
	default:
	    printf("Unknown joint type: %d in link '%s'\n", link->joint->type, link->name.c_str());
	    break;
	}
	
	if (!jtype.empty())
	{
	    TiXmlElement *joint = new TiXmlElement("joint:" + jtype);
	    
	    addKeyValue(joint, "body1", link->parentName);
	    addKeyValue(joint, "body2", link->name);
	    addKeyValue(joint, "anchor", link->name);
	    
	    addKeyValue(joint, "axis", values2str(3, link->joint->axis));
	    addKeyValue(joint, "axisOffset", values2str(3, link->joint->anchor));
	    
	    addKeyValue(joint, "lowStop", values2str(1, link->joint->limit));
	    addKeyValue(joint, "highStop", values2str(1, link->joint->limit + 1));
	    
	    /* add joint to document */
	    root->LinkEndChild(joint);
	}
    }
    
    for (unsigned int i = 0 ; i < link->children.size() ; ++i)
	convertLink(root, link->children[i], currentTransform);
}

void convert(robot_desc::URDF &wgxml, TiXmlDocument &doc)
{
    TiXmlDeclaration *decl = new TiXmlDeclaration("1.0", "", "");
    doc.LinkEndChild(decl);
    
    /* create root element and define needed namespaces */
    TiXmlElement *root = new TiXmlElement("model:physical");
    root->SetAttribute("xmlns:model", "http://playerstage.sourceforge.net/gazebo/xmlschema/#model");
    root->SetAttribute("xmlns:sensor", "http://playerstage.sourceforge.net/gazebo/xmlschema/#sensor");
    root->SetAttribute("xmlns:body", "http://playerstage.sourceforge.net/gazebo/xmlschema/#body");
    root->SetAttribute("xmlns:geom", "http://playerstage.sourceforge.net/gazebo/xmlschema/#geom");
    root->SetAttribute("xmlns:joint", "http://playerstage.sourceforge.net/gazebo/xmlschema/#joint");
    root->SetAttribute("xmlns:controller", "http://playerstage.sourceforge.net/gazebo/xmlschema/#controller");
    root->SetAttribute("xmlns:interface", "http://playerstage.sourceforge.net/gazebo/xmlschema/#interface");
    doc.LinkEndChild(root);
    
    /* set the transform for the whole model to identity */
    addKeyValue(root, "xyz", "0 0 0");
    addKeyValue(root, "rpy", "0 0 0");
    libTF::Pose3D transform;    
    
    for (unsigned int k = 0 ; k < wgxml.getDisjointPartCount() ; ++k)
	convertLink(root, wgxml.getDisjointPart(k), transform);
}

void usage(const char *progname)
{
    printf("\nUsage: %s URDF.xml Gazebo.model\n", progname);
    printf("       where URDF.xml is the file containing a robot description in the Willow Garage format (URDF)\n");
    printf("       and Gazebo.model is the file where the Gazebo model should be written\n\n");
}

int main(int argc, char **argv)
{
    if (argc != 3)
    {
	usage(argv[0]);
	exit(1);
    }
    
    robot_desc::URDF wgxml;

    if (!wgxml.loadFile(argv[1]))
    {
	printf("Unable to load robot model from %s\n", argv[1]);	
	exit(2);
    }
    
    TiXmlDocument doc;
    
    convert(wgxml, doc);
    
    if (!doc.SaveFile(argv[2]))
    {
	printf("Unable to save gazebo model in %s\n", argv[2]);	
	exit(3);
    }
    
    return 0;
}
