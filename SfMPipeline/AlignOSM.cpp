/*
 * Copyright (c) 2014. Jonathan Ventura. All rights reserved.
 * Licensed pursuant to the terms and conditions available for viewing at:
 * http://opensource.org/licenses/BSD-3-Clause
 *
 * File: AlignOSM.cpp
 * Author: Jonathan Ventura
 * Last Modified: 27.4.2014
 */

#include <MultiView/multiview.h>
#include <MultiView/multiview_io_xml.h>

#include "osm_xml_reader.h"

#include <opencv2/highgui.hpp>

#include <iostream>

using namespace vrlt;

Reconstruction r;
Node *root = NULL;

OSMData osmdata;

struct Transformation
{
    double centerX;
    double centerZ;
    double angle;    // radians
    double scale;
    Transformation() : centerX(0),centerZ(0),angle(0),scale(1) { }
    Eigen::Matrix4d matrix()
    {
        Eigen::Vector3d r;
        r << 0, angle, 0;
        Eigen::Matrix4d mat( Eigen::Matrix4d::Identity() );
        mat.block<3,3>(0,0) = Sophus::SO3d::exp(r).matrix();
        Eigen::Vector3d c;
        c << centerX,0,centerZ;
        mat.block<3,1>(0,3) = mat.block<3,3>(0,0)*(-c);
        Eigen::Vector4d s;
        s << scale,scale,scale,1;
        return Eigen::DiagonalMatrix<double, 4>(s)*mat;
    }
};

Transformation pointTransformation;
Transformation osmTransformation;

double metersToPixels = 1.0;

const int size = 1000;

void getPointLimits()
{
    double minX = INFINITY;
    double maxX = -INFINITY;
    double minZ = INFINITY;
    double maxZ = -INFINITY;
    
    std::vector<double> Xvalues;
    std::vector<double> Zvalues;
    Xvalues.reserve( root->points.size() );
    Zvalues.reserve( root->points.size() );
    
    for ( ElementList::iterator it = root->points.begin(); it != root->points.end(); it++ )
    {
        Point *point = (Point*)it->second;
        
        double X = point->position[0] / point->position[3];
        double Z = point->position[2] / point->position[3];
        
        Xvalues.push_back( X );
        Zvalues.push_back( Z );
    }
    
    std::sort( Xvalues.begin(), Xvalues.end() );
    std::sort( Zvalues.begin(), Zvalues.end() );
    
    minX = Xvalues[ Xvalues.size() * .1 ];
    pointTransformation.centerX = Xvalues[ Xvalues.size() * .5 ];
    maxX = Xvalues[ Xvalues.size() * .9 ];
    minZ = Zvalues[ Zvalues.size() * .1 ];
    pointTransformation.centerZ = Zvalues[ Zvalues.size() * .5 ];
    maxZ = Zvalues[ Zvalues.size() * .9 ];
    
    pointTransformation.scale = 100./(maxZ-minZ);
}

void renderPoints( cv::Mat &image )
{
    for ( ElementList::iterator it = root->points.begin(); it != root->points.end(); it++ )
    {
        Point *point = (Point*)it->second;
        
        double X = point->position[0] / point->position[3];
        double Z = point->position[2] / point->position[3];
        
        Eigen::Vector4d XYZ;
        XYZ << X,0,Z,1;
        
        XYZ = pointTransformation.matrix() * XYZ;
        
        cv::Point2i pt( size/2 + round(metersToPixels*XYZ[0]), size/2 - round(metersToPixels*XYZ[2]) );
        cv::circle( image, pt, 0, cv::Scalar(0) );
    }
}

void getOSMLimits()
{
    double minnorth = INFINITY;
    double maxnorth = -INFINITY;
    double mineast = INFINITY;
    double maxeast = -INFINITY;
    
    for ( OSMWayList::iterator it = osmdata.ways.begin(); it != osmdata.ways.end(); it++ )
    {
        OSMWay *way = it->second;
        if ( way->building == false ) continue;
        
        for ( size_t i = 0; i < way->nodeids.size(); i++ )
        {
            OSMNode *node = osmdata.nodes[way->nodeids[i]];
        
            if ( node->north < minnorth ) minnorth = node->north;
            if ( node->north > maxnorth ) maxnorth = node->north;
            if ( node->east < mineast ) mineast = node->east;
            if ( node->east > maxeast ) maxeast = node->east;
        }
    }
    
    osmTransformation.centerX = mineast+(maxeast-mineast)/2;
    osmTransformation.centerZ = minnorth+(maxnorth-minnorth)/2;
    metersToPixels = size/(maxeast-mineast);
}

void renderOSM( cv::Mat &image )
{
    for ( OSMWayList::iterator it = osmdata.ways.begin(); it != osmdata.ways.end(); it++ )
    {
        OSMWay *way = it->second;
        if ( way->building == false ) continue;
        
        for ( size_t i = 1; i < way->nodeids.size(); i++ )
        {
            OSMNode *node0 = osmdata.nodes[way->nodeids[i-1]];
            OSMNode *node1 = osmdata.nodes[way->nodeids[i]];
            
            Eigen::Vector4d XYZ0;
            XYZ0 << node0->east,0,node0->north,1;
            XYZ0 = osmTransformation.matrix() * XYZ0;
            
            Eigen::Vector4d XYZ1;
            XYZ1 << node1->east,0,node1->north,1;
            XYZ1 = osmTransformation.matrix() * XYZ1;

            cv::Point2i pt0( size/2 + round(metersToPixels*XYZ0[0]), size/2 - round(metersToPixels*XYZ0[2]) );
            cv::Point2i pt1( size/2 + round(metersToPixels*XYZ1[0]), size/2 - round(metersToPixels*XYZ1[2]) );
            cv::line( image, pt0, pt1, cv::Scalar(0) );
        }
    }
}

int main( int argc, char **argv )
{
    if ( argc != 4 ) {
        fprintf( stderr, "usage: %s <reconstruction> <osm xml file> <output>\n", argv[0] );
        return 0;
    }
    
    std::string pathin = std::string(argv[1]);
    std::string osmin = std::string(argv[2]);
    std::string pathout = std::string(argv[3]);
    
    XML::read( r, pathin );
    
    if ( r.nodes.find("root") == r.nodes.end() )
    {
        std::cerr << "error: no root node found.\n";
        exit(1);
    }
    root = (Node*)r.nodes["root"];
    
    getPointLimits();
    
    osmdata.read( osmin );
    std::cout << "OSM data has " << osmdata.nodes.size() << " nodes and " << osmdata.ways.size() << " ways\n";
    
    getOSMLimits();
    
    double bigstep = 1.;
    double smallstep = 0.1;
    
    cv::Mat mapimage;
    mapimage = cv::Mat( cv::Size( size, size ), CV_8UC1, cv::Scalar(255) );
    
    cv::namedWindow( "AlignOSM" );
    
    
    bool should_render = true;
    bool should_quit = false;
    while ( !should_quit )
    {
        int key = cv::waitKey( 1 );
        
        switch ( key )
        {
            case 'q':
                should_quit = true;
                break;
                
            case 'k':
                osmTransformation.centerZ += bigstep;
                should_render = true;
                break;

            case 'i':
                osmTransformation.centerZ -= bigstep;
                should_render = true;
                break;

            case 'K':
                osmTransformation.centerZ += smallstep;
                should_render = true;
                break;
                
            case 'I':
                osmTransformation.centerZ -= smallstep;
                should_render = true;
                break;

            case 'j':
                osmTransformation.centerX += bigstep;
                should_render = true;
                break;
                
            case 'l':
                osmTransformation.centerX -= bigstep;
                should_render = true;
                break;
                
            case 'J':
                osmTransformation.centerX += smallstep;
                should_render = true;
                break;
                
            case 'L':
                osmTransformation.centerX -= smallstep;
                should_render = true;
                break;

            case 'w':
                pointTransformation.scale *= 1.05;
                should_render = true;
                break;
                
            case 's':
                pointTransformation.scale /= 1.05;
                should_render = true;
                break;
                
            case 't':
                pointTransformation.angle += M_PI/180.;
                should_render = true;
                break;
                
            case 'r':
                pointTransformation.angle -= M_PI/180.;
                should_render = true;
                break;
                
            case 'a':
                metersToPixels *= 2;
                should_render = true;
                break;
                
            case 'z':
                metersToPixels /= 2;
                should_render = true;
                break;

            default:
                break;
        }
        
        if ( should_render )
        {
            mapimage = cv::Scalar(255);
            
            renderOSM( mapimage );
        
            renderPoints( mapimage );
            
            cv::imshow( "AlignOSM", mapimage );
            
            should_render = false;
        }
    }
    
    cv::imwrite( "Output/map.png", mapimage );
    
    return 0;
}