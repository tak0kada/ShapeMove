// ============================================================================
// SpinXForm -- Mesh.cpp
// Keenan Crane
// August 16, 2011
//

#include <fstream>
#include <sstream>
#include <cmath>
#include "MeshY.h"
#include "LinearSolverY.h"
#include "EigenSolverY.h"
#include "UtilityY.h"

namespace spiny{
extern cm::Common cc;
Mesh :: Mesh( void )
// default constructor
: L( cc ), E( cc ) // give matrices a handle to the CHOLMOD environment "cc"
{}

void Mesh :: updateDeformation( void )
{
   int t0 = clock();
cout << "a\n" << endl;
   // solve eigenvalue problem for local similarity transformation lambda
   buildEigenvalueProblem();
   cout << "b\n" << endl;

   EigenSolver::solve( E, lambda );
cout << "c\n" << endl;

   // solve Poisson problem for new vertex positions
   // (we assume the final degree of freedom equals zero
   // in order to get a strictly positive-definite matrix)
   buildPoissonProblem();
   int nV = vertices.size();
   vector<Quaternion> v( nV-1 );
   buildLaplacian();
   LinearSolver::solve( L, v, omega );
   for( int i = 0; i < nV-1; i++ )
   {
      newVertices[i] = v[i];
      //cout << vertices[i] << "\n" << endl;
   }
   newVertices[nV-1] = 0.;
   normalizeSolution();

   int t1 = clock();
   cout << "time: " << (t1-t0)/(double) CLOCKS_PER_SEC << "s" << endl;
}
void Mesh :: updateDeformation2( void )
{

   int nV = vertices.size();
   for( int i = 0; i < nV; i++ )
   {
      vertices[i] = newVertices[i];
   }
}

void Mesh :: resetDeformation( void )
{
   // copy original mesh vertices to current mesh
   for( size_t i = 0; i < vertices.size(); i++ )
   {
      newVertices[i] = vertices[i];
   }

   normalizeSolution();
}

void Mesh :: setCurvatureChange( const Image& image, const double scale )
// sets rho values by interpreting "image" as a square image
// in the range [0,1] x [0,1] and mapping values to the
// surface via vertex texture coordinates -- grayscale
// values in the  range [0,1] get mapped (linearly) to values
// in the range [-scale,scale]
{
   double w = (double) image.width();
   double h = (double) image.height();

   for( size_t i = 0; i < faces.size(); i++ )
   {
      // get handle to current value of rho
      rho[i] = 0.;

      // compute average value over the face
      for( int j = 0; j < 3; j++ )
      {
         Vector uv = faces[i].uv[j];
         rho[i] += image.sample( uv.x*w, uv.y*h ) / 3.;
      }

      // map value to range [-scale,scale]
      rho[i] = (2.*(rho[i]-.5)) * scale;
   }
}

void Mesh :: setCurvatureChange3( const double scale )
// sets rho values by interpreting "image" as a square image
// in the range [0,1] x [0,1] and mapping values to the
// surface via vertex texture coordinates -- grayscale
// values in the  range [0,1] get mapped (linearly) to values
// in the range [-scale,scale]
{
	//build Laplacian
   int nV = vertices.size();
   QuaternionMatrix L0; //SparseMatrix�ɂȂ��Ă���
   L0.resize( nV, nV );
   // visit each face
   for( size_t i = 0; i < faces.size(); i++ )
   {
      // visit each triangle corner
      for( int j = 0; j < 3; j++ )
      {
         // get vertex indices
         int k0 = faces[i].vertex[ (j+0) % 3 ];
         int k1 = faces[i].vertex[ (j+1) % 3 ];
         int k2 = faces[i].vertex[ (j+2) % 3 ];

         // get vertex positions
         Vector f0 = vertices[k0].im();
         Vector f1 = vertices[k1].im();
         Vector f2 = vertices[k2].im();

         // compute cotangent of the angle at the current vertex
         // (equal to cosine over sine, which equals the dot
         // product over the norm of the cross product)
         Vector u1 = f1 - f0;
         Vector u2 = f2 - f0;
         double cotAlpha = (u1*u2)/(u1^u2).norm();

         // add contribution of this cotangent to the matrix
         // ��[���̃Z�������l���o�^�����d�g��(�a�s��)�ɂȂ��Ă��邩��傫���Ȃ肷���Ȃ�
         L0( k1, k2 ) -= cotAlpha / 2.;
         L0( k2, k1 ) -= cotAlpha / 2.;
         L0( k1, k1 ) += cotAlpha / 2.;
         L0( k2, k2 ) += cotAlpha / 2.;
      }
   }
   vector<Quaternion> rhoQ( faces.size(), 0. );
     vector<Quaternion> Lf = L0.multV(vertices);
     
     int minn =0;
     int maxn =0;

vector<double> rho(faces.size());
for(size_t i=0;i<faces.size();i++){
	rho[i] = 1.0;
	      // get handle to current value of rho
      //rhoQ[i] = 0.;

      // compute average value over the face
      for( size_t j = 0; j < 3; j++ )
      {
         if(faces[i].vertex[j]<minn){
			minn = faces[i].vertex[j];
		}
		if(faces[i].vertex[j]>maxn){
			maxn = faces[i].vertex[j];
		}
         rhoQ[i] += Lf[faces[i].vertex[j]] / 3.;
         //rhoQ[i] += L0(faces[i].vertex[j],faces[i].vertex[j]) / 3.;
      }
            rho[i] = (rhoQ[i].im()).norm();
            Quaternion p1 = vertices[ faces[i].vertex[0] ];
      Quaternion p2 = vertices[ faces[i].vertex[1] ];
      Quaternion p3 = vertices[ faces[i].vertex[2] ];

      if(((( p2-p1 ) * ( p3-p1 )) * rhoQ[i]).re() < 0)
      {
		rho[i] *= (-1.0)*scale/area(i);
		//cout << "negative" << "\n";
	}else{
		//cout << "positive" << "\n";
		rho[i] *= scale/area(i);
	}
	
	
}
removeMean(rho);

double mi = 0.0;
double ma = 0.0;
for(size_t i = 0;i < faces.size();i++)
{
	if(mi > rho[i]){
		mi = rho[i];
	}
	if(ma < rho[i]){
		ma = rho[i];
	}
}
if(-mi > ma){
	ma = -mi;
}
for(size_t i =0;i<faces.size();i++){
	rho[i] *= 1.0/ma;
	cout << rho[i] << endl;
}
}

double Mesh :: area( int i )
// returns area of triangle i in the original mesh
{
   Vector& p1 = vertices[ faces[i].vertex[0] ].im();
   Vector& p2 = vertices[ faces[i].vertex[1] ].im();
   Vector& p3 = vertices[ faces[i].vertex[2] ].im();

   return .5 * (( p2-p1 ) ^ ( p3-p1 )).norm();
}

void Mesh :: buildEigenvalueProblem( void )
{
   // allocate a sparse |V|x|V| matrix
   int nV = vertices.size();
   QuaternionMatrix E0;
   E0.resize( nV, nV );
   cout << "E0:"  << "\n";

   // visit each face
   for( size_t k = 0; k < faces.size(); k++ )
   {
      double A = area(k);
      //cout << A << endl;
      double a = -1. / (4.*A);
      double b = rho[k] / 6.;
      double c = A*rho[k]*rho[k] / 9.;

      // get vertex indices
      int I[3] =
      {
         faces[k].vertex[0],
         faces[k].vertex[1],
         faces[k].vertex[2]
      };

      // compute edges across from each vertex
      Quaternion e[3];
      for( int i = 0; i < 3; i++ )
      {
         e[i] = vertices[ I[ (i+2) % 3 ]] -
                vertices[ I[ (i+1) % 3 ]] ;
      }

      // increment matrix entry for each ordered pair of vertices
      for( int i = 0; i < 3; i++ )
      for( int j = 0; j < 3; j++ )
      {
         E0(I[i],I[j]) += a*e[i]*e[j] + b*(e[j]-e[i]) + c;
      }
   }

   // build Cholesky factorization
   E.build( E0.toReal() );
}

void Mesh :: buildPoissonProblem( void )
{
   buildOmega();
}

void Mesh :: buildLaplacian( void )
// builds the cotan-Laplace operator, where the final row and
// column are omitted to make the system strictly positive-
// definite (equivalent to setting the final degree of freedom
// to zero)
{
   // allocate a sparse |V|x|V| matrix
   int nV = vertices.size();
   QuaternionMatrix L0;
   L0.resize( nV-1, nV-1 );

   // visit each face
   for( size_t i = 0; i < faces.size(); i++ )
   {
      // visit each triangle corner
      for( int j = 0; j < 3; j++ )
      {
         // get vertex indices
         int k0 = faces[i].vertex[ (j+0) % 3 ];
         int k1 = faces[i].vertex[ (j+1) % 3 ];
         int k2 = faces[i].vertex[ (j+2) % 3 ];

         // get vertex positions
         Vector f0 = vertices[k0].im();
         Vector f1 = vertices[k1].im();
         Vector f2 = vertices[k2].im();

         // compute cotangent of the angle at the current vertex
         // (equal to cosine over sine, which equals the dot
         // product over the norm of the cross product)
         Vector u1 = f1 - f0;
         Vector u2 = f2 - f0;
         double cotAlpha = (u1*u2)/(u1^u2).norm();

         // add contribution of this cotangent to the matrix
         if( k1 != nV-1 && k2 != nV-1 ) L0( k1, k2 ) -= cotAlpha / 2.;
         if( k2 != nV-1 && k1 != nV-1 ) L0( k2, k1 ) -= cotAlpha / 2.;
         if( k1 != nV-1 && k1 != nV-1 ) L0( k1, k1 ) += cotAlpha / 2.;
         if( k2 != nV-1 && k2 != nV-1 ) L0( k2, k2 ) += cotAlpha / 2.;
      }
   }
   
   // build Cholesky factorization
   L.build( L0.toReal() );
}

void Mesh :: buildOmega( void )
{
   int nV = vertices.size();

   // clear omega
   for( size_t i = 0; i < omega.size(); i++ )
   {
      omega[i] = 0.;
   }

   // visit each face
   for( size_t i = 0; i < faces.size(); i++ )
   {
      // get indices of the vertices of this face
      int v[3] = { faces[i].vertex[0],
                   faces[i].vertex[1],
                   faces[i].vertex[2] };

      // visit each edge
      for( int j = 0; j < 3; j++ )
      {
         // get vertices
         Quaternion f0 = vertices[ v[ (j+0) % 3 ]];
         Quaternion f1 = vertices[ v[ (j+1) % 3 ]];
         Quaternion f2 = vertices[ v[ (j+2) % 3 ]];

         // determine orientation of this edge
         int a = v[ (j+1) % 3 ];
         int b = v[ (j+2) % 3 ];
         if( a > b )
         {
            swap( a, b );
         }

         // compute transformed edge vector
         Quaternion lambda1 = lambda[a];
         Quaternion lambda2 = lambda[b];
         Quaternion e = vertices[b] - vertices[a];
         Quaternion eTilde = (1./3.) * (~lambda1) * e * lambda1 +
                             (1./6.) * (~lambda1) * e * lambda2 +
                             (1./6.) * (~lambda2) * e * lambda1 +
                             (1./3.) * (~lambda2) * e * lambda2 ;

         // compute cotangent of the angle opposite the current edge
         Vector u1 = ( f1 - f0 ).im();
         Vector u2 = ( f2 - f0 ).im();
         double cotAlpha = (u1*u2)/(u1^u2).norm();

         // add contribution of this edge to the divergence at its vertices
         if( a != nV-1 ) omega[a] -= cotAlpha * eTilde / 2.;
         if( b != nV-1 ) omega[b] += cotAlpha * eTilde / 2.;
      }
   }
}

void Mesh :: normalizeSolution( void )
{
   // center vertices around the origin
   removeMean( newVertices );

   // find the vertex with the largest norm
   double r = 0.;
   for( size_t i = 0; i < vertices.size(); i++ )
   {
      r = max( r, newVertices[i].norm2() );
   }
   r = sqrt(r);

   // rescale so that vertices have norm at most one
   for( size_t i = 0; i < vertices.size(); i++ )
   {
      newVertices[i] /= r;
   }
}

std::vector<double> Mesh :: returnRho()
{
	return rho;
}

// FILE I/O --------------------------------------------------------------------

void Mesh :: read( const string& filename )
// loads a triangle mesh in Wavefront OBJ format
{
   // open mesh file
   ifstream in( filename.c_str() );
   if( !in.is_open() )
   {
      cerr << "Error: couldn't open file ";
      cerr << filename;
      cerr << " for input!" << endl;
      exit( 1 );
   }

   // temporary list of vertex coordinates
   vector<Vector> uv;

   // parse mesh file
   string s;
   while( getline( in, s ))
   {
      stringstream line( s );
      string token;

      line >> token;

      if( token == "v" ) // vertex
      {
         double x, y, z;

         line >> x >> y >> z;

         vertices.push_back( Quaternion( 0., x, y, z ));
         newVertices.push_back( Quaternion( 0., x, y, z ));
      }
      if( token == "vt" ) // texture coordinate
      {
         double u, v;

         line >> u >> v;

         uv.push_back( Vector( u, v, 0. ));
      }
      else if( token == "f" ) // face
      {
         Face triangle;

         // iterate over vertices
         for( int i = 0; i < 3; i++ )
         {
            line >> s;
            stringstream item( s );

            int I[3] = { -1, -1, -1 };

            // iterate over v, vt, and vn indices
            for( int j = 0; getline( item, s, '/' ) && j < 3; j++ )
            {
               stringstream index( s );
               index >> I[j];
            }

            triangle.vertex[i] = I[0]-1;

            if( I[1] != -1 )
            {
               triangle.uv[i] = uv[ I[1]-1 ];
            }
         }

         faces.push_back( triangle );
      }
   }

   // allocate space for mesh attributes
   lambda.resize( vertices.size() );
   omega.resize( vertices.size()-1 );
   rho.resize( faces.size() );
   normalizeSolution();

   // prefactor Laplace matrix
   buildLaplacian();
}

void Mesh :: write( const string& filename )
// saves a triangle mesh in Wavefront OBJ format
{
   ofstream out( filename.c_str() );

   if( !out.is_open() )
   {
      cerr << "Error: couldn't open file ";
      cerr << filename;
      cerr << " for output!" << endl;
      return;
   }

   for( size_t i = 0; i < vertices.size(); i++ )
   {
      out << "v " << newVertices[i].im().x << " "
                  << newVertices[i].im().y << " "
                  << newVertices[i].im().z << endl;
   }

   for( size_t i = 0; i < faces.size(); i++ )
   {
      out << "f " << 1+faces[i].vertex[0] << " "
                  << 1+faces[i].vertex[1] << " "
                  << 1+faces[i].vertex[2] << endl;
   }
}
}