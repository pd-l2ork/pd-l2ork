/***********************************************************************
 * dummy disis_spi for non-RPi platforms
 * ****************************************************************************/
#include "m_pd.h"

static t_class *disis_spi_class;

typedef struct _disis_spi
{
    t_object x_obj;
    t_outlet *x_out1;
    t_outlet *x_out2;
    t_outlet *x_out3;
    t_outlet *x_out4;
    t_outlet *x_out5;
    t_outlet *x_out6;
    t_outlet *x_out7;
    t_outlet *x_out8;
    t_outlet *x_out9;
} t_disis_spi;

static void disis_spi_open(t_disis_spi *spi, t_symbol *devspi){

}
 
static int disis_spi_close(t_disis_spi *spi){

}

static void disis_spi_free(t_disis_spi *spi){

}


static void disis_spi_bang(t_disis_spi *spi)
{

}

static t_disis_spi *disis_spi_new(t_symbol *devspi){
    post("WARNING: this is a dummy version of the disis_spi object because you appear to be running it on a platform other than Raspberry Pi.");

    t_disis_spi *spi = (t_disis_spi *)pd_new(disis_spi_class);

    spi->x_out1 = outlet_new(&spi->x_obj, gensym("float"));
    spi->x_out2 = outlet_new(&spi->x_obj, gensym("float"));
    spi->x_out3 = outlet_new(&spi->x_obj, gensym("float"));
    spi->x_out4 = outlet_new(&spi->x_obj, gensym("float"));
    spi->x_out5 = outlet_new(&spi->x_obj, gensym("float"));
    spi->x_out6 = outlet_new(&spi->x_obj, gensym("float"));
    spi->x_out7 = outlet_new(&spi->x_obj, gensym("float"));
    spi->x_out8 = outlet_new(&spi->x_obj, gensym("float"));
    spi->x_out9 = outlet_new(&spi->x_obj, gensym("float"));
 
    return(spi);
}


void disis_spi_setup(void)
{
    disis_spi_class = class_new(gensym("disis_spi"), (t_newmethod)disis_spi_new,
        (t_method)disis_spi_free, sizeof(t_disis_spi), 0, A_DEFSYM, 0);
    class_addmethod(disis_spi_class, (t_method)disis_spi_open, gensym("open"), 
        A_DEFSYM, 0);
    class_addmethod(disis_spi_class, (t_method)disis_spi_close, gensym("close"), 
        0, 0);
    //class_addfloat(disis_gpio_class, disis_gpio_float); (later do sending data back to the spi)
    class_addbang(disis_spi_class, disis_spi_bang);
}
