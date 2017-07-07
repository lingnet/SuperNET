
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
//  LP_commands.c
//  marketmaker
//


char *stats_JSON(void *ctx,char *myipaddr,int32_t pubsock,cJSON *argjson,char *remoteaddr,uint16_t port) // from rpc port
{
    char *method,*ipaddr,*userpass,*base,*rel,*coin,*retstr = 0; uint16_t argport=0,pushport,subport; int32_t otherpeers,othernumutxos,flag = 0; struct LP_peerinfo *peer; cJSON *retjson,*reqjson = 0; struct iguana_info *ptr;
    //printf("stats_JSON(%s)\n",jprint(argjson,0));
    if ( (ipaddr= jstr(argjson,"ipaddr")) != 0 && (argport= juint(argjson,"port")) != 0 )
    {
        if ( strcmp(ipaddr,"127.0.0.1") != 0 && argport >= 1000 )
        {
            flag = 1;
            if ( (pushport= juint(argjson,"push")) == 0 )
                pushport = argport + 1;
            if ( (subport= juint(argjson,"sub")) == 0 )
                subport = argport + 2;
            if ( (peer= LP_peerfind((uint32_t)calc_ipbits(ipaddr),argport)) != 0 )
            {
                if ( 0 && (otherpeers= jint(argjson,"numpeers")) > peer->numpeers )
                    peer->numpeers = otherpeers;
                if ( 0 && (othernumutxos= jint(argjson,"numutxos")) > peer->numutxos )
                {
                    printf("change.(%s) numutxos.%d -> %d mynumutxos.%d\n",peer->ipaddr,peer->numutxos,othernumutxos,LP_mypeer != 0 ? LP_mypeer->numutxos:0);
                    peer->numutxos = othernumutxos;
                }
                //printf("peer.(%s) found (%d %d) (%d %d) (%s)\n",peer->ipaddr,peer->numpeers,peer->numutxos,otherpeers,othernumutxos,jprint(argjson,0));
            } else LP_addpeer(LP_mypeer,LP_mypubsock,ipaddr,argport,pushport,subport,jint(argjson,"numpeers"),jint(argjson,"numutxos"));
        }
    }
    if ( (method= jstr(argjson,"method")) == 0 )
    {
        if ( flag == 0 || jobj(argjson,"result") != 0 )
            printf("stats_JSON no method: (%s) (%s:%u)\n",jprint(argjson,0),ipaddr,argport);
        return(0);
    }
    /*if ( strcmp(method,"hello") == 0 )
    {
        //printf("got hello from %s:%u\n",ipaddr!=0?ipaddr:"",argport);
        return(0);
    }
    else*/ if ( strcmp(method,"message") == 0 )
    {
        static char *laststr;
        char *newstr; bits256 pubkey = jbits256(argjson,"pubkey");
        if ( bits256_nonz(pubkey) == 0 || bits256_cmp(pubkey,LP_mypub25519) == 0 )
        {
            newstr = jprint(argjson,0);
            if ( laststr == 0 || strcmp(laststr,newstr) != 0 )
            {
                printf("got message.(%s) from %s:%u\n",newstr,ipaddr!=0?ipaddr:"",argport);
                if ( laststr != 0 )
                    free(laststr);
                laststr = newstr;
            }
        }
        return(0);
    }
    //else if ( strcmp(method,"nn_tests") == 0 )
    //    return(clonestr("{\"result\":\"success\"}"));
    else if ( strcmp(method,"help") == 0 )
        return(clonestr("{\"result\":\" \
available localhost RPC commands:\n \
setprice(base, rel, price)\n\
myprice(base, rel)\n\
enable(coin)\n\
disable(coin)\n\
inventory(coin)\n\
bestfit(rel, relvolume)\n\
ordermatch(base, txfee=0, rel, desttxfee=0, price, txid, vout, feetxid, feevout, duration=3600)\n\
trade(price, timeout=10, duration=3600, <quotejson returned from ordermatch>)\n\
autotrade(base, rel, price, relvolume, timeout=10, duration=3600)\n\
swapstatus()\n\
swapstatus(requestid, quoteid)\n\
public API:\n \
getcoins()\n\
getpeers()\n\
getutxos()\n\
getutxos(coin, lastn)\n\
orderbook(base, rel, duration=3600)\n\
getprices(base, rel)\n\
sendmessage(base=coin, rel="", pubkey=zero, <argjson method2>)\n\
trust(pubkey, trust)\n\
\"}"));
    
    /*
    register(pubkey,pushaddr)\n\
    registerall(numnodes)\n\
    lookup(pubkey)\n\
    forward(pubkey,method2,<argjson>)\n\
    forward(pubkey,method2=publish,<argjson>)\n\
    forwardhex(pubkey,hex)\n\
     */

    base = jstr(argjson,"base");
    rel = jstr(argjson,"rel");
    if ( USERPASS[0] != 0 && strcmp(remoteaddr,"127.0.0.1") == 0 && port != 0 )
    {
        if ( USERPASS_COUNTER == 0 )
        {
            USERPASS_COUNTER = 1;
            retjson = cJSON_CreateObject();
            jaddstr(retjson,"userpass",USERPASS);
            jaddbits256(retjson,"mypubkey",LP_mypub25519);
            jadd(retjson,"coins",LP_coinsjson());
            return(jprint(retjson,1));
        }
        if ( (userpass= jstr(argjson,"userpass")) == 0 || strcmp(userpass,USERPASS) != 0 )
            return(clonestr("{\"error\":\"authentication error\"}"));
        jdelete(argjson,"userpass");
        if ( strcmp(method,"sendmessage") == 0 )
        {
            if ( (reqjson= LP_dereference(argjson,"sendmessage")) != 0 )
            {
                LP_broadcast_message(LP_mypubsock,base!=0?base:jstr(argjson,"coin"),rel,jbits256(argjson,"pubkey"),jprint(reqjson,1));
                return(clonestr("{\"result\":\"success\"}"));
            } else return(clonestr("{\"error\":\"couldnt dereference sendmessage\"}"));
        }
        if ( base != 0 && rel != 0 )
        {
            double price,bid,ask;
            if ( LP_isdisabled(base,rel) != 0 )
                return(clonestr("{\"error\":\"at least one of coins disabled\"}"));
            price = jdouble(argjson,"price");
            if ( strcmp(method,"setprice") == 0 )
            {
                if ( price > SMALLVAL )
                {
                    if ( LP_mypriceset(base,rel,price) < 0 )
                        return(clonestr("{\"error\":\"couldnt set price\"}"));
                    else return(LP_pricepings(ctx,myipaddr,LP_mypubsock,base,rel,price * LP_profitratio));
                } else return(clonestr("{\"error\":\"no price\"}"));
            }
            else if ( strcmp(method,"myprice") == 0 )
            {
                if ( LP_myprice(&bid,&ask,base,rel) > SMALLVAL )
                {
                    retjson = cJSON_CreateObject();
                    jaddstr(retjson,"base",base);
                    jaddstr(retjson,"rel",rel);
                    jaddnum(retjson,"bid",bid);
                    jaddnum(retjson,"ask",ask);
                    return(jprint(retjson,1));
                } else return(clonestr("{\"error\":\"no price set\"}"));
            }
            else if ( strcmp(method,"ordermatch") == 0 )
            {
                if ( price > SMALLVAL )
                return(LP_ordermatch(base,j64bits(argjson,"txfee"),price,rel,jbits256(argjson,"txid"),jint(argjson,"vout"),jbits256(argjson,"feetxid"),jint(argjson,"feevout"),j64bits(argjson,"desttxfee"),jint(argjson,"duration")));
                else return(clonestr("{\"error\":\"no price set\"}"));
            }
            else if ( strcmp(method,"trade") == 0 )
            {
                struct LP_quoteinfo Q;
                if ( price > SMALLVAL || jobj(argjson,"quote") != 0 )
                {
                    LP_quoteparse(&Q,jobj(argjson,"quote"));
                    return(LP_trade(ctx,myipaddr,pubsock,&Q,price,jint(argjson,"timeout"),jint(argjson,"duration")));
                } else return(clonestr("{\"error\":\"no price set or no quote object\"}"));
            }
            else if ( strcmp(method,"autotrade") == 0 )
            {
                if ( price > SMALLVAL )
                {
                    printf("price set (%s/%s) <- %.8f\n",rel,base,1./price);
                    LP_mypriceset(rel,base,1./price);
                    return(LP_autotrade(ctx,myipaddr,pubsock,base,rel,price,jdouble(argjson,"relvolume"),jint(argjson,"timeout"),jint(argjson,"duration")));
                } else return(clonestr("{\"error\":\"no price set\"}"));
            }
        }
        else if ( rel != 0 && strcmp(method,"bestfit") == 0 )
        {
            double relvolume;
            if ( (relvolume= jdouble(argjson,"relvolume")) > SMALLVAL )
                return(LP_bestfit(rel,relvolume));
            else return(clonestr("{\"error\":\"no relvolume set\"}"));
        }
        else if ( (coin= jstr(argjson,"coin")) != 0 )
        {
            if ( strcmp(method,"enable") == 0 )
            {
                if ( (ptr= LP_coinsearch(coin)) != 0 )
                    ptr->inactive = 0;
                return(jprint(LP_coinsjson(),1));
            }
            else if ( strcmp(method,"disable") == 0 )
            {
                if ( (ptr= LP_coinsearch(coin)) != 0 )
                    ptr->inactive = (uint32_t)time(NULL);
                return(jprint(LP_coinsjson(),1));
            }
            if ( LP_isdisabled(coin,0) != 0 )
                return(clonestr("{\"error\":\"coin is disabled\"}"));
            if ( strcmp(method,"inventory") == 0 )
            {
                struct iguana_info *ptr; bits256 privkey,pubkey; uint8_t pubkey33[33];
                if ( (ptr= LP_coinfind(coin)) != 0 )
                {
                    privkey = LP_privkeycalc(ctx,pubkey33,&pubkey,ptr,"",USERPASS_WIFSTR);
                    //LP_utxopurge(0);
                    LP_privkey_init(-1,ptr,privkey,pubkey,pubkey33);
                    retjson = cJSON_CreateObject();
                    jaddstr(retjson,"result","success");
                    jaddstr(retjson,"coin",coin);
                    jaddnum(retjson,"timestamp",time(NULL));
                    jadd(retjson,"alice",LP_inventory(coin,0));
                    jadd(retjson,"bob",LP_inventory(coin,1));
                    return(jprint(retjson,1));
                }
            }
        }
        else if ( strcmp(method,"swapstatus") == 0 )
        {
            uint32_t requestid,quoteid;
            if ( (requestid= juint(argjson,"requestid")) != 0 && (quoteid= juint(argjson,"quoteid")) != 0 )
                return(basilisk_swapentry(requestid,quoteid));
            else return(basilisk_swaplist());
        }
        else if ( strcmp(method,"myprices") == 0 )
            return(LP_myprices());
        else if ( strcmp(method,"trust") == 0 )
            return(LP_pubkey_trustset(jbits256(argjson,"pubkey"),jint(argjson,"trust")));
    }
    if ( IAMLP == 0 )
    {
        if ( (reqjson= LP_dereference(argjson,"broadcast")) != 0 )
        {
            if ( jobj(reqjson,"method2") != 0 )
            {
                jdelete(reqjson,"method");
                method = jstr(reqjson,"method2");
                jaddstr(reqjson,"method",method);
                //jdelete(reqjson,"method2");
                //printf("BROADCASTED.(%s)\n",jprint(reqjson,0));
                printf("BROADCASTED METHOD.%s\n",method);
            }
            argjson = reqjson;
        }
    }
    if ( LP_isdisabled(base,rel) != 0 )
        return(clonestr("{\"result\":\"at least one of coins disabled\"}"));
    else if ( LP_isdisabled(jstr(argjson,"coin"),0) != 0 )
        retstr = clonestr("{\"result\":\"coin is disabled\"}");
    else if ( strcmp(method,"reserved") == 0 )
        retstr = LP_quotereceived(argjson);
    else if ( strcmp(method,"connected") == 0 )
        retstr = LP_connectedalice(argjson);
    else if ( strcmp(method,"checktxid") == 0 )
        retstr = LP_spentcheck(argjson);
    else if ( strcmp(method,"getcoins") == 0 )
        return(jprint(LP_coinsjson(),1));
    else if ( strcmp(method,"postprice") == 0 )
        retstr = LP_postedprice(argjson);
    //else if ( strcmp(method,"broadcast") == 0 )
    //    retstr = LP_broadcasted(argjson);
    else if ( strcmp(method,"getprices") == 0 )
        return(LP_prices());
    else if ( strcmp(method,"orderbook") == 0 )
        return(LP_orderbook(base,rel,jint(argjson,"duration")));
    else if ( strcmp(method,"registerall") == 0 )
    {
        return(clonestr("{\"error\":\"you are running an obsolete version, update\"}"));
        //return(LP_registerall(jint(argjson,"numnodes")));
    }
    else if ( strcmp(method,"forward") == 0 )
    {
        return(clonestr("{\"error\":\"you are running an obsolete version, update\"}"));
        /*cJSON *reqjson;
        if ( (reqjson= LP_dereference(argjson,"forward")) != 0 )
        {
            //printf("FORWARDED.(%s)\n",jprint(argjson,0));
            if ( LP_forward(ctx,myipaddr,pubsock,jbits256(argjson,"pubkey"),jprint(reqjson,1),1) > 0 )
                retstr = clonestr("{\"result\":\"success\"}");
            else retstr = clonestr("{\"result\":\"error forwarding\"}");
        } else retstr = clonestr("{\"result\":\"cant recurse forwards\"}");
        return(retstr);*/
    }
    else if ( strcmp(method,"keepalive") == 0 )
    {
        return(clonestr("{\"error\":\"you are running an obsolete version, update\"}"));
        /*printf("got keepalive lag.%d switch.%u\n",(int32_t)time(NULL) - LP_deadman_switch,LP_deadman_switch);
        LP_deadman_switch = (uint32_t)time(NULL);
        return(clonestr("{\"result\":\"success\"}"));*/
    }
    else if ( strcmp(method,"getpeers") == 0 )
        return(LP_peers());
    else if ( strcmp(method,"getutxos") == 0 )
        return(LP_utxos(1,LP_mypeer,jstr(argjson,"coin"),jint(argjson,"lastn")));
    else if ( strcmp(method,"utxo") == 0 )
    {
        printf("UTXO.%s\n",method);
        if ( LP_utxoaddjson(1,LP_mypubsock,argjson) != 0 )
            retstr = clonestr("{\"result\":\"success\",\"utxo\":\"received\"}");
        else retstr = clonestr("{\"result\":\"couldnt add utxo\"}");
    }
    else
    {
        if ( IAMLP != 0 )
        {
            if ( strcmp(method,"register") == 0 )
            {
                return(clonestr("{\"error\":\"you are running an obsolete version, update\"}"));
                /*retstr = LP_register(jbits256(argjson,"client"),jstr(argjson,"pushaddr"),juint(argjson,"pushport"));
                //printf("got (%s) from register\n",retstr!=0?retstr:"");
                return(retstr);*/
            }
            else if ( strcmp(method,"lookup") == 0 )
            {
                return(clonestr("{\"error\":\"you are running an obsolete version, update\"}"));
                //return(LP_lookup(jbits256(argjson,"client")));
            }
            if ( strcmp(method,"broadcast") == 0 )
            {
                bits256 zero; char *cipherstr; int32_t cipherlen; uint8_t cipher[LP_ENCRYPTED_MAXSIZE];
                if ( (reqjson= LP_dereference(argjson,"broadcast")) != 0 )
                {
                    if ( (cipherstr= jstr(reqjson,"cipherstr")) != 0 )
                    {
                        cipherlen = (int32_t)strlen(cipherstr) >> 1;
                        if ( cipherlen <= sizeof(cipher) )
                        {
                            decode_hex(cipher,cipherlen,cipherstr);
                            LP_queuesend(calc_crc32(0,&cipher[2],cipherlen-2),LP_mypubsock,base,rel,cipher,cipherlen);
                        } else retstr = clonestr("{\"error\":\"cipherstr too big\"}");
                    }
                    else
                    {
                        memset(zero.bytes,0,sizeof(zero));
                        LP_broadcast_message(LP_mypubsock,base!=0?base:jstr(argjson,"coin"),rel,zero,jprint(reqjson,0));
                    }
                    retstr = clonestr("{\"result\":\"success\"}");
                } else retstr = clonestr("{\"error\":\"couldnt dereference sendmessage\"}");
                //retstr = LP_forwardhex(ctx,pubsock,jbits256(argjson,"pubkey"),jstr(argjson,"hex"));
            }
            else if ( strcmp(method,"psock") == 0 )
            {
                if ( myipaddr == 0 || myipaddr[0] == 0 || strcmp(myipaddr,"127.0.0.1") == 0 )
                {
                    if ( LP_mypeer != 0 )
                        myipaddr = LP_mypeer->ipaddr;
                    else printf("LP_psock dont have actual ipaddr?\n");
                }
                if ( jint(argjson,"ispaired") != 0 )
                    return(LP_psock(myipaddr,jint(argjson,"ispaired")));
                else return(clonestr("{\"error\":\"you are running an obsolete version, update\"}"));
            }
            else if ( strcmp(method,"notify") == 0 )
                retstr = clonestr("{\"result\":\"success\",\"notify\":\"received\"}");
        }
        else
        {
            /*if ( strcmp(method,"register") == 0 )
            {
                //printf("nonLP got (%s)\n",jprint(argjson,0));
                retstr = clonestr("{\"result\":\"success\",\"register\":\"received\"}");
            }*/
        }
    }
    printf("ERROR.(%s)\n",jprint(argjson,0));
    if ( reqjson != 0 )
        free_json(reqjson);
    if ( retstr != 0 )
    {
        free(retstr);
        return(0);
    }
    return(0);
}
