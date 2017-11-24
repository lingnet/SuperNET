
/******************************************************************************
 * Copyright © 2014-2017 The SuperNET Developers.                             *
 *                                                                            *
 * See the AUTHORS, DEVELOPER-AGREEMENT and LICENSE files at                  *
 * the top-level directory of this distribution for the individual copyright  *
 * holder information and the developer policies on copyright and licensing.  *
 *                                                                            *
 * Unless otherwise agreed in a custom licensing agreement, no part of the    *
 * SuperNET software, including this file may be copied, modified, propagated *
 * or distributed except according to the terms contained in the LICENSE file *
 *                                                                            *
 * Removal or modification of this copyright notice is prohibited.            *
 *                                                                            *
 ******************************************************************************/

//
//  LP_zeroconf.c
//  marketmaker
//

int32_t LP_deposit_addr(char *p2shaddr,uint8_t *script,uint8_t taddr,uint8_t p2shtype,uint32_t timestamp,uint8_t *pubsecp33)
{
    uint8_t elsepub33[33],p2sh_rmd160[20]; int32_t n;
    decode_hex(elsepub33,33,BOTS_BONDPUBKEY33);
    n = bitcoin_performancebond(p2sh_rmd160,script,0,timestamp,pubsecp33,elsepub33);
    bitcoin_address(p2shaddr,taddr,p2shtype,script,n);
    return(n);
}

char *LP_zeroconf_deposit(struct iguana_info *coin,int32_t weeks,double amount,int32_t broadcast)
{
    char p2shaddr[64],*retstr,*hexstr; uint8_t script[512]; int32_t weeki,scriptlen; cJSON *argjson,*retjson,*array,*item,*obj; uint32_t timestamp; bits256 txid,sendtxid; uint64_t amount64;
    if ( strcmp(coin->symbol,"KMD") != 0 )
        return(clonestr("{\"error\":\"zeroconf deposit must be in KMD\"}"));
    if ( amount < 10.0 )
        return(clonestr("{\"error\":\"minimum zeroconf deposit is 10 KMD\"}"));
    if ( weeks < 0 || weeks > 52 )
        return(clonestr("{\"error\":\"weeks must be between 0 and 52\"}"));
    if ( weeks > 0 )
    {
        timestamp = (uint32_t)time(NULL);
        timestamp /= LP_WEEKMULT;
        timestamp += weeks+1;
        timestamp *= LP_WEEKMULT;
        weeki = (timestamp - LP_FIRSTWEEKTIME) / LP_WEEKMULT;
        if ( weeks >= 10000 )
            return(clonestr("{\"error\":\"numweeks must be less than 10000\"}"));
    } else timestamp = (uint32_t)time(NULL) + 300, weeki = 0;
    scriptlen = LP_deposit_addr(p2shaddr,script,coin->taddr,coin->p2shtype,timestamp,G.LP_pubsecp);
    argjson = cJSON_CreateObject();
    array = cJSON_CreateArray();
    item = cJSON_CreateObject();
    jaddnum(item,p2shaddr,amount);
    jaddi(array,item);
    item = cJSON_CreateObject();
    amount64 = (amount * SATOSHIDEN) / 1000;
    amount64 = (amount64 / 10000) * 10000 + weeki;
    jaddnum(item,BOTS_BONDADDRESS,dstr(amount64));
    jaddi(array,item);
    item = cJSON_CreateObject();
    jaddnum(item,coin->smartaddr,0.0001);
    jaddi(array,item);
    jadd(argjson,"outputs",array);
    //printf("deposit.(%s)\n",jprint(argjson,0));
    if ( (retstr= LP_withdraw(coin,argjson)) != 0 )
    {
        if ( (retjson= cJSON_Parse(retstr)) != 0 )
        {
            if ( jobj(retjson,"result") != 0 )
                jdelete(retjson,"result");
            jaddstr(retjson,"address",p2shaddr);
            jaddnum(retjson,"expiration",timestamp);
            jaddnum(retjson,"deposit",amount);
            if ( (obj= jobj(retjson,"complete")) != 0 && is_cJSON_True(obj) != 0 && (hexstr= jstr(retjson,"hex")) != 0 )
            {
                txid = jbits256(retjson,"txid");
                if ( broadcast != 0 )
                {
                    if (bits256_nonz(txid) != 0 )
                    {
                        sendtxid = LP_broadcast("deposit","KMD",hexstr,txid);
                        if ( bits256_cmp(sendtxid,txid) != 0 )
                        {
                            jaddstr(retjson,"error","broadcast txid mismatch");
                            jaddbits256(retjson,"broadcast",sendtxid);
                            free(retstr);
                            return(jprint(retjson,1));
                        }
                        else
                        {
                            jaddstr(retjson,"result","success");
                            jaddbits256(retjson,"broadcast",sendtxid);
                            free(retstr);
                            return(jprint(retjson,1));
                        }
                    }
                    else
                    {
                        jaddstr(retjson,"error","couldnt broadcast since no txid created");
                        free(retstr);
                        return(jprint(retjson,1));
                    }
                }
                else
                {
                    jaddstr(retjson,"result","success");
                    free(retstr);
                    return(jprint(retjson,1));
                }
            }
            else
            {
                jaddstr(retjson,"error","couldnt create deposit txid");
                free(retstr);
                return(jprint(retjson,1));
            }
            free_json(retjson);
        }
        free(retstr);
    }
    return(clonestr("{\"error\":\"error with LP_withdraw for zeroconf deposit\"}"));
}

char *LP_zeroconf_claim(struct iguana_info *coin,char *depositaddr,uint32_t expiration)
{
    static void *ctx;
    uint8_t redeemscript[512],userdata[64]; char vinaddr[64],str[65],*signedtx=0; uint32_t timestamp,now,redeemlen,claimtime; int32_t i,n,height,utxovout,userdatalen; bits256 signedtxid,utxotxid,sendtxid; int64_t sum,destamount,satoshis; cJSON *array,*item,*txids,*retjson;
    if ( ctx == 0 )
        ctx = bitcoin_ctx();
    if ( strcmp(coin->symbol,"KMD") != 0 )
        return(clonestr("{\"error\":\"zeroconf deposit must be in KMD\"}"));
    now = (uint32_t)time(NULL);
    sum = 0;
    txids = cJSON_CreateArray();
    timestamp = (now / LP_WEEKMULT) * LP_WEEKMULT + LP_WEEKMULT;
    while ( timestamp > LP_FIRSTWEEKTIME )
    {
        if ( expiration != 0 )
            timestamp = expiration;
        else timestamp -= LP_WEEKMULT;
        redeemlen = LP_deposit_addr(vinaddr,redeemscript,coin->taddr,coin->p2shtype,timestamp,G.LP_pubsecp);
        if ( strcmp(depositaddr,vinaddr) == 0 )
        {
            claimtime = (uint32_t)time(NULL)-777;
            if ( claimtime <= timestamp )
            {
                printf("claimtime.%u vs locktime.%u, need to wait %d seconds\n",claimtime,timestamp,(int32_t)timestamp-claimtime);
            }
            else
            {
                printf("found %s at timestamp.%u\n",vinaddr,timestamp);
                if ( (array= LP_listunspent(coin->symbol,vinaddr)) != 0 )
                {
                    userdata[0] = 0x51;
                    userdatalen = 1;
                    utxovout = 0;
                    //printf("unspents.(%s)\n",jprint(array,0));
                    if ( (n= cJSON_GetArraySize(array)) > 0 )
                    {
                        for (i=0; i<n; i++)
                        {
                            item = jitem(array,i);
                            satoshis = LP_listunspent_parseitem(coin,&utxotxid,&utxovout,&height,item);
                            printf("satoshis %.8f %s/v%d\n",dstr(satoshis),bits256_str(str,utxotxid),utxovout);
                            if ( (signedtx= basilisk_swap_bobtxspend(&signedtxid,10000,"zeroconfclaim",coin->symbol,coin->wiftaddr,coin->taddr,coin->pubtype,coin->p2shtype,coin->isPoS,coin->wiftype,ctx,G.LP_privkey,0,redeemscript,redeemlen,userdata,userdatalen,utxotxid,utxovout,coin->smartaddr,G.LP_pubsecp,0,claimtime,&destamount,0,0,vinaddr,1,coin->zcash)) != 0 )
                            {
                                printf("signedtx.(%s)\n",signedtx);
                                sendtxid = LP_broadcast("claim","KMD",signedtx,signedtxid);
                                if ( bits256_cmp(sendtxid,signedtxid) == 0 )
                                {
                                    jaddibits256(txids,sendtxid);
                                    sum += (satoshis-coin->txfee);
                                }
                                else printf("error sending %s\n",bits256_str(str,signedtxid));
                                free(signedtx);
                            } else printf("error claiming zeroconf deposit %s/v%d %.8f\n",bits256_str(str,utxotxid),utxovout,dstr(satoshis));
                        }
                    }
                    free_json(array);
                    retjson = cJSON_CreateObject();
                    jaddstr(retjson,"result","success");
                    jaddnum(retjson,"claimed",dstr(sum));
                    jadd(retjson,"txids",txids);
                    return(jprint(retjson,1));
                }
            }
        }
        if ( expiration != 0 )
            break;
    }
    return(clonestr("{\"error\":\"no zeroconf deposits to claim\"}"));
}

void LP_zeroconf_credit(int32_t dispflag,char *coinaddr,int64_t satoshis,int32_t weeki,char *p2shaddr)
{
    uint32_t timestamp; struct LP_address *ap; struct iguana_info *coin = LP_coinfind("KMD");
    if ( coin != 0 )
    {
        timestamp = LP_FIRSTWEEKTIME + weeki*LP_WEEKMULT;
        if (  time(NULL) < timestamp-60*3600 && (ap= LP_address(coin,coinaddr)) != 0 )
        {
            ap->zeroconf_credits += satoshis;
            if ( dispflag != 0 )
                printf("ZEROCONF credit.(%s) %.8f weeki.%d (%s) -> sum %.8f\n",coinaddr,dstr(satoshis),weeki,p2shaddr,dstr(ap->zeroconf_credits));
        }
    }
}

void LP_zeroconf_deposits(struct iguana_info *coin)
{
    static int dispflag = 1;
    cJSON *array,*item,*txjson,*vouts,*v,*txobj; int32_t i,n,numvouts,height,vout,weeki; bits256 txid; char destaddr[64],p2shaddr[64]; struct LP_address *ap,*tmp; int64_t satoshis,amount64;
    HASH_ITER(hh,coin->addresses,ap,tmp)
    {
        ap->zeroconf_credits = 0;
    }
    if ( (array= LP_listreceivedbyaddress("KMD",BOTS_BONDADDRESS)) != 0 )
    {
        //printf("ZEROCONF.(%s)\n",jprint(array,0));
        if ( (n= cJSON_GetArraySize(array)) > 0 )
        {
            for (i=0; i<n; i++)
            {
                if ( coin->electrum != 0 )
                {
                    item = jitem(array,i);
                    LP_listunspent_parseitem(coin,&txid,&vout,&height,item);
                } else txid = jbits256i(array,i);
                if ( (txjson= LP_gettx(coin->symbol,txid)) != 0 )
                {
                    // vout0 deposit, vout1 botsfee, vout2 smartaddress
                    if ( (vouts= jarray(&numvouts,txjson,"vout")) > 0 && numvouts >= 3 && LP_destaddr(destaddr,jitem(vouts,2)) == 0 )
                    {
                        amount64 = LP_value_extract(jitem(vouts,1),0);
                        weeki = (amount64 % 10000);
                        v = jitem(vouts,0);
                        satoshis = LP_value_extract(v,0);
                        //printf("%s funded %.8f weeki.%d\n",destaddr,dstr(satoshis),weeki);
                        if ( LP_destaddr(p2shaddr,v) == 0 )
                        {
                            if ( (txobj= LP_gettxout(coin->symbol,p2shaddr,txid,0)) != 0 )
                            {
                                free_json(txobj);
                                LP_zeroconf_credit(dispflag,destaddr,satoshis,weeki,p2shaddr);
                            }
                        }
                    }
                    free_json(txjson);
                }
            }
        }
        free_json(array);
    }
    dispflag = 0;
}

int64_t LP_dynamictrust(bits256 pubkey,int64_t kmdvalue)
{
    struct LP_pubswap *ptr,*tmp; struct LP_swapstats *sp; struct LP_pubkey_info *pubp; struct LP_address *ap; char coinaddr[64]; struct iguana_info *coin; int64_t swaps_kmdvalue = 0;
    if ( (coin= LP_coinfind("KMD")) != 0 && (pubp= LP_pubkeyfind(pubkey)) != 0 )
    {
        bitcoin_address(coinaddr,coin->taddr,coin->pubtype,pubp->pubsecp,33);
        if ((ap= LP_address(coin,coinaddr)) != 0 )//&& ap->zeroconf_credits >= kmdvalue )
        {
            DL_FOREACH_SAFE(pubp->bobswaps,ptr,tmp)
            {
                if ( (sp= ptr->swap) != 0 && sp->finished == 0 && sp->expired == 0 )
                    swaps_kmdvalue += LP_kmdvalue(sp->Q.srccoin,sp->Q.satoshis);
            }
            DL_FOREACH_SAFE(pubp->aliceswaps,ptr,tmp)
            {
                if ( (sp= ptr->swap) != 0 && sp->finished == 0 && sp->expired == 0 )
                    swaps_kmdvalue += LP_kmdvalue(sp->Q.destcoin,sp->Q.destsatoshis);
            }
            //printf("%s zeroconf_credits %.8f vs (%.8f + current %.8f)\n",coinaddr,dstr(ap->zeroconf_credits),dstr(swaps_kmdvalue),dstr(kmdvalue));
            //if ( ap->zeroconf_credits > swaps_kmdvalue+kmdvalue )
                return(ap->zeroconf_credits - (swaps_kmdvalue+kmdvalue));
        }
    }
    return(0);
}
