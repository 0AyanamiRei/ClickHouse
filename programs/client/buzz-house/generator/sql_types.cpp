#include "sql_types.h"
#include "fuzz_timezones.h"
#include "hugeint.h"
#include "statement_generator.h"
#include "uhugeint.h"

namespace buzzhouse
{

const SQLType * TypeDeepCopy(const SQLType * tp)
{
    const IntType * it;
    const FloatType * ft;
    const DateType * dt;
    const DateTimeType * dtt;
    const DecimalType * decp;
    const StringType * st;
    const EnumType * et;
    const DynamicType * ddt;
    const JSONType * jt;
    const Nullable * nl;
    const LowCardinality * lc;
    const GeoType * gt;
    const ArrayType * at;
    const MapType * mt;
    const TupleType * ttp;
    const VariantType * vtp;
    const NestedType * ntp;

    if (dynamic_cast<const BoolType *>(tp))
    {
        return new BoolType();
    }
    else if ((it = dynamic_cast<const IntType *>(tp)))
    {
        return new IntType(it->size, it->is_unsigned);
    }
    else if ((ft = dynamic_cast<const FloatType *>(tp)))
    {
        return new FloatType(ft->size);
    }
    else if ((dt = dynamic_cast<const DateType *>(tp)))
    {
        return new DateType(dt->extended);
    }
    else if ((dtt = dynamic_cast<const DateTimeType *>(tp)))
    {
        return new DateTimeType(dtt->extended, dtt->precision, dtt->timezone);
    }
    else if ((decp = dynamic_cast<const DecimalType *>(tp)))
    {
        return new DecimalType(decp->precision, decp->scale);
    }
    else if ((st = dynamic_cast<const StringType *>(tp)))
    {
        return new StringType(st->precision);
    }
    else if (dynamic_cast<const UUIDType *>(tp))
    {
        return new UUIDType();
    }
    else if (dynamic_cast<const IPv4Type *>(tp))
    {
        return new IPv4Type();
    }
    else if (dynamic_cast<const IPv6Type *>(tp))
    {
        return new IPv6Type();
    }
    else if ((et = dynamic_cast<const EnumType *>(tp)))
    {
        return new EnumType(et->size, et->values);
    }
    else if ((ddt = dynamic_cast<const DynamicType *>(tp)))
    {
        return new DynamicType(ddt->ntypes);
    }
    else if ((jt = dynamic_cast<const JSONType *>(tp)))
    {
        return new JSONType(jt->desc);
    }
    else if ((nl = dynamic_cast<const Nullable *>(tp)))
    {
        return new Nullable(TypeDeepCopy(nl->subtype));
    }
    else if ((lc = dynamic_cast<const LowCardinality *>(tp)))
    {
        return new LowCardinality(TypeDeepCopy(lc->subtype));
    }
    else if ((gt = dynamic_cast<const GeoType *>(tp)))
    {
        return new GeoType(gt->geo_type);
    }
    else if ((at = dynamic_cast<const ArrayType *>(tp)))
    {
        return new ArrayType(TypeDeepCopy(at->subtype));
    }
    else if ((mt = dynamic_cast<const MapType *>(tp)))
    {
        return new MapType(TypeDeepCopy(mt->key), TypeDeepCopy(mt->value));
    }
    else if ((ttp = dynamic_cast<const TupleType *>(tp)))
    {
        std::vector<const SubType> subtypes;

        for (const auto & entry : ttp->subtypes)
        {
            subtypes.push_back(SubType(entry.cname, TypeDeepCopy(entry.subtype)));
        }
        return new TupleType(std::move(subtypes));
    }
    else if ((vtp = dynamic_cast<const VariantType *>(tp)))
    {
        std::vector<const SQLType *> subtypes;

        for (const auto & entry : vtp->subtypes)
        {
            subtypes.push_back(TypeDeepCopy(entry));
        }
        return new VariantType(std::move(subtypes));
    }
    else if ((ntp = dynamic_cast<const NestedType *>(tp)))
    {
        std::vector<const NestedSubType> subtypes;

        for (const auto & entry : ntp->subtypes)
        {
            subtypes.push_back(NestedSubType(entry.cname, TypeDeepCopy(entry.subtype)));
        }
        return new NestedType(std::move(subtypes));
    }
    else
    {
        assert(0);
    }
    return nullptr;
}

std::tuple<const SQLType *, sql_query_grammar::Integers>
StatementGenerator::RandomIntType(RandomGenerator & rg, const uint32_t allowed_types)
{
    assert(this->ids.empty());

    if ((allowed_types & allow_unsigned_int))
    {
        if ((allowed_types & allow_int8))
        {
            this->ids.push_back(1);
        }
        this->ids.push_back(2);
        this->ids.push_back(3);
        this->ids.push_back(4);
        if ((allowed_types & allow_hugeint))
        {
            this->ids.push_back(5);
            this->ids.push_back(6);
        }
    }
    if ((allowed_types & allow_int8))
    {
        this->ids.push_back(7);
    }
    this->ids.push_back(8);
    this->ids.push_back(9);
    this->ids.push_back(10);
    if ((allowed_types & allow_hugeint))
    {
        this->ids.push_back(11);
        this->ids.push_back(12);
    }
    const uint32_t nopt = rg.PickRandomlyFromVector(this->ids);
    this->ids.clear();
    switch (nopt)
    {
        case 1:
            return std::make_tuple(new IntType(8, true), sql_query_grammar::Integers::UInt8);
        case 2:
            return std::make_tuple(new IntType(16, true), sql_query_grammar::Integers::UInt16);
        case 3:
            return std::make_tuple(new IntType(32, true), sql_query_grammar::Integers::UInt32);
        case 4:
            return std::make_tuple(new IntType(64, true), sql_query_grammar::Integers::UInt64);
        case 5:
            return std::make_tuple(new IntType(128, true), sql_query_grammar::Integers::UInt128);
        case 6:
            return std::make_tuple(new IntType(256, true), sql_query_grammar::Integers::UInt256);
        case 7:
            return std::make_tuple(new IntType(8, false), sql_query_grammar::Integers::Int8);
        case 8:
            return std::make_tuple(new IntType(16, false), sql_query_grammar::Integers::Int16);
        case 9:
            return std::make_tuple(new IntType(32, false), sql_query_grammar::Integers::Int32);
        case 10:
            return std::make_tuple(new IntType(64, false), sql_query_grammar::Integers::Int64);
        case 11:
            return std::make_tuple(new IntType(128, false), sql_query_grammar::Integers::Int128);
        case 12:
            return std::make_tuple(new IntType(256, false), sql_query_grammar::Integers::Int256);
        default:
            assert(0);
    }
}

std::tuple<const SQLType *, sql_query_grammar::FloatingPoints> StatementGenerator::RandomFloatType(RandomGenerator & rg)
{
    const bool use32 = rg.NextBool();
    return std::make_tuple(
        new FloatType(use32 ? 32 : 64), use32 ? sql_query_grammar::FloatingPoints::Float32 : sql_query_grammar::FloatingPoints::Float64);
}

std::tuple<const SQLType *, sql_query_grammar::Dates> StatementGenerator::RandomDateType(RandomGenerator & rg, const uint32_t allowed_types)
{
    const bool use32 = (allowed_types & allow_date32) && rg.NextBool();
    return std::make_tuple(new DateType(use32), use32 ? sql_query_grammar::Dates::Date32 : sql_query_grammar::Dates::Date);
}

const SQLType *
StatementGenerator::RandomDateTimeType(RandomGenerator & rg, const uint32_t allowed_types, sql_query_grammar::DateTimeTp * dt)
{
    const bool use64 = (allowed_types & allow_datetime64) && rg.NextBool();
    std::optional<uint32_t> precision = std::nullopt;
    std::optional<std::string> timezone = std::nullopt;

    if (dt)
    {
        dt->set_type(use64 ? sql_query_grammar::DateTimes::DateTime64 : sql_query_grammar::DateTimes::DateTime);
    }
    if (use64 && rg.NextSmallNumber() < 5)
    {
        precision = std::optional<uint32_t>(rg.NextSmallNumber() - 1);
        if (dt)
        {
            dt->set_precision(precision.value());
        }
    }
    if (rg.NextSmallNumber() < 5)
    {
        timezone = std::optional<std::string>(rg.PickRandomlyFromVector(timezones));
        if (dt)
        {
            dt->set_timezone(timezone.value());
        }
    }
    return new DateTimeType(use64, precision, timezone);
}

const SQLType * StatementGenerator::BottomType(
    RandomGenerator & rg, const uint32_t allowed_types, const bool low_card, sql_query_grammar::BottomTypeName * tp)
{
    const SQLType * res = nullptr;

    const uint32_t int_type
        = 40,
        floating_point_type = 10 * static_cast<uint32_t>((allowed_types & allow_floating_points) != 0 && this->fc.fuzz_floating_points),
        date_type = 15 * static_cast<uint32_t>((allowed_types & allow_dates) != 0),
        datetime_type = 15 * static_cast<uint32_t>((allowed_types & allow_datetimes) != 0),
        string_type = 30 * static_cast<uint32_t>((allowed_types & allow_strings) != 0),
        decimal_type = 20 * static_cast<uint32_t>(!low_card && (allowed_types & allow_decimals) != 0),
        bool_type = 20 * static_cast<uint32_t>(!low_card && (allowed_types & allow_bool) != 0),
        enum_type = 20 * static_cast<uint32_t>(!low_card && (allowed_types & allow_enum) != 0),
        uuid_type = 10 * static_cast<uint32_t>(!low_card && (allowed_types & allow_uuid) != 0),
        ipv4_type = 5 * static_cast<uint32_t>(!low_card && (allowed_types & allow_ipv4) != 0),
        ipv6_type = 5 * static_cast<uint32_t>(!low_card && (allowed_types & allow_ipv6) != 0),
        json_type = 20 * static_cast<uint32_t>(!low_card && (allowed_types & allow_json) != 0),
        dynamic_type = 30 * static_cast<uint32_t>(!low_card && (allowed_types & allow_dynamic) != 0),
        prob_space = int_type + floating_point_type + date_type + datetime_type + string_type + decimal_type + bool_type + enum_type
        + uuid_type + ipv4_type + ipv6_type + json_type + dynamic_type;
    std::uniform_int_distribution<uint32_t> next_dist(1, prob_space);
    const uint32_t nopt = next_dist(rg.gen);

    if (int_type && nopt < (int_type + 1))
    {
        sql_query_grammar::Integers nint;

        std::tie(res, nint) = RandomIntType(rg, allowed_types);
        if (tp)
        {
            tp->set_integers(nint);
        }
    }
    else if (floating_point_type && nopt < (int_type + floating_point_type + 1))
    {
        sql_query_grammar::FloatingPoints nflo;

        std::tie(res, nflo) = RandomFloatType(rg);
        if (tp)
        {
            tp->set_floats(nflo);
        }
    }
    else if (date_type && nopt < (int_type + floating_point_type + date_type + 1))
    {
        sql_query_grammar::Dates dd;

        std::tie(res, dd) = RandomDateType(rg, allowed_types);
        if (tp)
        {
            tp->set_dates(dd);
        }
    }
    else if (datetime_type && nopt < (int_type + floating_point_type + date_type + datetime_type + 1))
    {
        sql_query_grammar::DateTimeTp * dtp = tp ? tp->mutable_datetimes() : nullptr;

        res = RandomDateTimeType(rg, low_card ? (allowed_types & ~(allow_datetime64)) : allowed_types, dtp);
    }
    else if (string_type && nopt < (int_type + floating_point_type + date_type + datetime_type + string_type + 1))
    {
        std::optional<uint32_t> swidth = std::nullopt;

        if (rg.NextBool())
        {
            if (tp)
            {
                tp->set_sql_string(true);
            }
        }
        else
        {
            swidth = std::optional<uint32_t>(rg.NextBool() ? rg.NextSmallNumber() : (rg.NextRandomUInt32() % 100));
            if (tp)
            {
                tp->set_fixed_string(swidth.value());
            }
        }
        res = new StringType(swidth);
    }
    else if (decimal_type && nopt < (int_type + floating_point_type + date_type + datetime_type + string_type + decimal_type + 1))
    {
        sql_query_grammar::Decimal * dec = tp ? tp->mutable_decimal() : nullptr;
        std::optional<uint32_t> precision = std::nullopt, scale = std::nullopt;

        if (rg.NextBool())
        {
            precision = std::optional<uint32_t>((rg.NextRandomUInt32() % 10) + 1);

            if (dec)
            {
                dec->set_precision(precision.value());
            }
            if (rg.NextBool())
            {
                scale = std::optional<uint32_t>(rg.NextRandomUInt32() % (precision.value() + 1));
                if (dec)
                {
                    dec->set_scale(scale.value());
                }
            }
        }
        res = new DecimalType(precision, scale);
    }
    else if (bool_type && nopt < (int_type + floating_point_type + date_type + datetime_type + string_type + decimal_type + bool_type + 1))
    {
        if (tp)
        {
            tp->set_boolean(true);
        }
        res = new BoolType();
    }
    else if (
        enum_type
        && nopt < (int_type + floating_point_type + date_type + datetime_type + string_type + decimal_type + bool_type + enum_type + 1))
    {
        const bool bits = rg.NextBool();
        std::vector<const EnumValue> evs;
        const uint32_t nvalues = (rg.NextLargeNumber() % enum_values.size()) + 1;
        sql_query_grammar::EnumDef * edef = tp ? tp->mutable_enum_def() : nullptr;

        edef->set_bits(bits);
        std::shuffle(enum_values.begin(), enum_values.end(), rg.gen);
        for (uint32_t i = 0; i < nvalues; i++)
        {
            const std::string & nval = enum_values[i];
            const int32_t num = static_cast<const int32_t>(bits ? rg.NextRandomInt16() : rg.NextRandomInt8());

            if (edef)
            {
                sql_query_grammar::EnumDefValue * edf = i == 0 ? edef->mutable_first_value() : edef->add_other_values();

                edf->set_number(num);
                edf->set_enumv(nval);
            }
            evs.push_back(EnumValue(nval, num));
        }
        res = new EnumType(bits ? 16 : 8, evs);
    }
    else if (
        uuid_type
        && nopt
            < (int_type + floating_point_type + date_type + datetime_type + string_type + decimal_type + bool_type + enum_type + uuid_type
               + 1))
    {
        if (tp)
        {
            tp->set_uuid(true);
        }
        res = new UUIDType();
    }
    else if (
        ipv4_type
        && nopt
            < (int_type + floating_point_type + date_type + datetime_type + string_type + decimal_type + bool_type + enum_type + uuid_type
               + ipv4_type + 1))
    {
        if (tp)
        {
            tp->set_ipv4(true);
        }
        res = new IPv4Type();
    }
    else if (
        ipv6_type
        && nopt
            < (int_type + floating_point_type + date_type + datetime_type + string_type + decimal_type + bool_type + enum_type + uuid_type
               + ipv4_type + ipv6_type + 1))
    {
        if (tp)
        {
            tp->set_ipv6(true);
        }
        res = new IPv6Type();
    }
    else if (
        json_type
        && nopt
            < (int_type + floating_point_type + date_type + datetime_type + string_type + decimal_type + bool_type + enum_type + uuid_type
               + ipv4_type + ipv6_type + json_type + 1))
    {
        std::string desc = "";
        sql_query_grammar::JsonDef * jdef = tp ? tp->mutable_json() : nullptr;
        const uint32_t nclauses = rg.NextMediumNumber() % 7;

        if (nclauses)
        {
            desc += "(";
        }
        for (uint32_t i = 0; i < nclauses; i++)
        {
            const uint32_t noption = rg.NextSmallNumber();
            sql_query_grammar::JsonDefItem * jdi = tp ? jdef->add_spec() : nullptr;

            if (i != 0)
            {
                desc += ", ";
            }
            if (noption < 4)
            {
                const uint32_t max_dpaths = rg.NextBool() ? (rg.NextSmallNumber() % 5) : (rg.NextRandomUInt32() % 1025);

                if (tp)
                {
                    jdi->set_max_dynamic_paths(max_dpaths);
                }
                desc += "max_dynamic_paths=";
                desc += std::to_string(max_dpaths);
            }
            else if (this->depth >= this->fc.max_depth || noption < 8)
            {
                const uint32_t max_dtypes = rg.NextBool() ? (rg.NextSmallNumber() % 5) : (rg.NextRandomUInt32() % 33);

                if (tp)
                {
                    jdi->set_max_dynamic_types(max_dtypes);
                }
                desc += "max_dynamic_types=";
                desc += std::to_string(max_dtypes);
            } /*else if (this->depth >= this->fc.max_depth || noption < 9) {
                const uint32_t nskips = (rg.NextMediumNumber() % 4) + 1;
                sql_query_grammar::ColumnPath *cp = tp ? jdi->mutable_skip_path() : nullptr;

                desc += "SKIP ";
                for (uint32_t j = 0 ; j < nskips; j++) {
                    std::string nbuf;
                    sql_query_grammar::Column *col = tp ? (j == 0 ? cp->mutable_col() : cp->add_sub_cols()) : nullptr;

                    if (j != 0) {
                        desc += ".";
                    }
                    desc += '`';
                    rg.NextJsonCol(nbuf);
                    desc += nbuf;
                    desc += '`';
                    if (tp) {
                        col->set_column(std::move(nbuf));
                    }
                }
            }*/
            else
            {
                uint32_t col_counter = 0;
                const uint32_t ncols = (rg.NextMediumNumber() % 4) + 1;
                sql_query_grammar::JsonPathType * jpt = tp ? jdi->mutable_path_type() : nullptr;
                sql_query_grammar::ColumnPath * cp = tp ? jpt->mutable_col() : nullptr;

                for (uint32_t j = 0; j < ncols; j++)
                {
                    std::string nbuf;
                    sql_query_grammar::Column * col = tp ? (j == 0 ? cp->mutable_col() : cp->add_sub_cols()) : nullptr;

                    if (j != 0)
                    {
                        desc += ".";
                    }
                    desc += '`';
                    rg.NextJsonCol(nbuf);
                    desc += nbuf;
                    desc += '`';
                    if (tp)
                    {
                        col->set_column(std::move(nbuf));
                    }
                }
                this->depth++;
                desc += " ";
                const SQLType * jtp = RandomNextType(rg, ~(allow_nested | allow_enum), col_counter, tp ? jpt->mutable_type() : nullptr);
                jtp->TypeName(desc, false);
                delete jtp;
                this->depth--;
            }
        }
        if (nclauses)
        {
            desc += ")";
        }
        res = new JSONType(std::move(desc));
    }
    else if (
        dynamic_type
        && nopt
            < (int_type + floating_point_type + date_type + datetime_type + string_type + decimal_type + bool_type + enum_type + uuid_type
               + ipv4_type + ipv6_type + json_type + dynamic_type + 1))
    {
        sql_query_grammar::Dynamic * dyn = tp ? tp->mutable_dynamic() : nullptr;
        std::optional<uint32_t> ntypes = std::nullopt;

        if (rg.NextBool())
        {
            ntypes = std::optional<uint32_t>(rg.NextBool() ? rg.NextSmallNumber() : ((rg.NextRandomUInt32() % 100) + 1));
            if (dyn)
            {
                dyn->set_ntypes(ntypes.value());
            }
        }
        res = new DynamicType(ntypes);
    }
    else
    {
        assert(0);
    }
    return res;
}

const SQLType * StatementGenerator::GenerateArraytype(
    RandomGenerator & rg, const uint32_t allowed_types, uint32_t & col_counter, sql_query_grammar::TopTypeName * tp)
{
    this->depth++;
    const SQLType * k = this->RandomNextType(rg, allowed_types, col_counter, tp);
    this->depth--;
    return new ArrayType(k);
}

const SQLType * StatementGenerator::GenerateArraytype(RandomGenerator & rg, const uint32_t allowed_types)
{
    uint32_t col_counter = 0;

    return GenerateArraytype(rg, allowed_types, col_counter, nullptr);
}

const SQLType * StatementGenerator::RandomNextType(
    RandomGenerator & rg, const uint32_t allowed_types, uint32_t & col_counter, sql_query_grammar::TopTypeName * tp)
{
    const uint32_t non_nullable_type = 50, nullable_type = 30 * static_cast<uint32_t>((allowed_types & allow_nullable) != 0),
                   array_type = 10 * static_cast<uint32_t>((allowed_types & allow_array) != 0 && this->depth < this->fc.max_depth),
                   map_type = 10
        * static_cast<uint32_t>((allowed_types & allow_map) != 0 && this->depth < this->fc.max_depth && this->width < this->fc.max_width),
                   tuple_type = 10 * static_cast<uint32_t>((allowed_types & allow_tuple) != 0 && this->depth < this->fc.max_depth),
                   variant_type = 10 * static_cast<uint32_t>((allowed_types & allow_variant) != 0 && this->depth < this->fc.max_depth),
                   nested_type = 10
        * static_cast<uint32_t>((allowed_types & allow_nested) != 0 && this->depth < this->fc.max_depth
                                && this->width < this->fc.max_width),
                   geo_type = 10 * static_cast<uint32_t>((allowed_types & allow_geo) != 0),
                   prob_space
        = nullable_type + non_nullable_type + array_type + map_type + tuple_type + variant_type + nested_type + geo_type;
    std::uniform_int_distribution<uint32_t> next_dist(1, prob_space);
    const uint32_t nopt = next_dist(rg.gen);

    if (non_nullable_type && nopt < (non_nullable_type + 1))
    {
        //non nullable
        const bool lcard = (allowed_types & allow_low_cardinality) && rg.NextMediumNumber() < 18;
        const SQLType * res
            = BottomType(rg, allowed_types, lcard, tp ? (lcard ? tp->mutable_non_nullable_lcard() : tp->mutable_non_nullable()) : nullptr);
        return lcard ? new LowCardinality(res) : res;
    }
    else if (nullable_type && nopt < (non_nullable_type + nullable_type + 1))
    {
        //nullable
        const bool lcard = (allowed_types & allow_low_cardinality) && rg.NextMediumNumber() < 18;
        const SQLType * res = new Nullable(BottomType(
            rg,
            allowed_types & ~(allow_dynamic | allow_json),
            lcard,
            tp ? (lcard ? tp->mutable_nullable_lcard() : tp->mutable_nullable()) : nullptr));
        return lcard ? new LowCardinality(res) : res;
    }
    else if (array_type && nopt < (nullable_type + non_nullable_type + array_type + 1))
    {
        //array
        const uint32_t nallowed = allowed_types & ~(allow_nested | ((allowed_types & allow_nullable_inside_array) ? 0 : allow_nullable));
        return GenerateArraytype(rg, nallowed, col_counter, tp ? tp->mutable_array() : nullptr);
    }
    else if (map_type && nopt < (nullable_type + non_nullable_type + array_type + map_type + 1))
    {
        //map
        sql_query_grammar::MapType * mt = tp ? tp->mutable_map() : nullptr;

        this->depth++;
        const SQLType * k
            = this->RandomNextType(rg, allowed_types & ~(allow_nullable | allow_nested), col_counter, mt ? mt->mutable_key() : nullptr);
        this->width++;
        const SQLType * v = this->RandomNextType(rg, allowed_types & ~(allow_nested), col_counter, mt ? mt->mutable_value() : nullptr);
        this->depth--;
        this->width--;
        return new MapType(k, v);
    }
    else if (tuple_type && nopt < (nullable_type + non_nullable_type + array_type + map_type + tuple_type + 1))
    {
        //tuple
        std::vector<const SubType> subtypes;
        const bool with_names = rg.NextBool();
        sql_query_grammar::TupleType * tt = tp ? tp->mutable_tuple() : nullptr;
        sql_query_grammar::TupleWithColumnNames * twcn = (tp && with_names) ? tt->mutable_with_names() : nullptr;
        sql_query_grammar::TupleWithOutColumnNames * twocn = (tp && !with_names) ? tt->mutable_no_names() : nullptr;
        const uint32_t ncols
            = this->width >= this->fc.max_width ? 0 : (rg.NextMediumNumber() % std::min<uint32_t>(6, this->fc.max_width - this->width));

        this->depth++;
        for (uint32_t i = 0; i < ncols; i++)
        {
            std::optional<uint32_t> opt_cname = std::nullopt;
            sql_query_grammar::TypeColumnDef * tcd = twcn ? twcn->add_values() : nullptr;
            sql_query_grammar::TopTypeName * ttn = twocn ? twocn->add_values() : nullptr;

            if (tcd)
            {
                const uint32_t ncname = col_counter++;

                tcd->mutable_col()->set_column("c" + std::to_string(ncname));
                opt_cname = std::optional<uint32_t>(ncname);
            }
            const SQLType * k
                = this->RandomNextType(rg, allowed_types & ~(allow_nested), col_counter, tcd ? tcd->mutable_type_name() : ttn);
            subtypes.push_back(SubType(opt_cname, k));
        }
        this->depth--;
        return new TupleType(subtypes);
    }
    else if (variant_type && nopt < (nullable_type + non_nullable_type + array_type + map_type + tuple_type + variant_type + 1))
    {
        //variant
        std::vector<const SQLType *> subtypes;
        sql_query_grammar::TupleWithOutColumnNames * twocn = tp ? tp->mutable_variant() : nullptr;
        const uint32_t ncols
            = this->width >= this->fc.max_width ? 0 : (rg.NextMediumNumber() % std::min<uint32_t>(6, this->fc.max_width - this->width));

        this->depth++;
        for (uint32_t i = 0; i < ncols; i++)
        {
            sql_query_grammar::TopTypeName * ttn = tp ? twocn->add_values() : nullptr;

            subtypes.push_back(this->RandomNextType(
                rg, allowed_types & ~(allow_nullable | allow_nested | allow_variant | allow_dynamic), col_counter, ttn));
        }
        this->depth--;
        return new VariantType(subtypes);
    }
    else if (
        nested_type && nopt < (nullable_type + non_nullable_type + array_type + map_type + tuple_type + variant_type + nested_type + 1))
    {
        //nested
        std::vector<const NestedSubType> subtypes;
        sql_query_grammar::NestedType * nt = tp ? tp->mutable_nested() : nullptr;
        const uint32_t ncols = (rg.NextMediumNumber() % (std::min<uint32_t>(5, this->fc.max_width - this->width))) + UINT32_C(1);

        this->depth++;
        for (uint32_t i = 0; i < ncols; i++)
        {
            const uint32_t cname = col_counter++;
            sql_query_grammar::TypeColumnDef * tcd = tp ? ((i == 0) ? nt->mutable_type1() : nt->add_others()) : nullptr;

            if (tcd)
            {
                tcd->mutable_col()->set_column("c" + std::to_string(cname));
            }
            const SQLType * k
                = this->RandomNextType(rg, allowed_types & ~(allow_nested), col_counter, tcd ? tcd->mutable_type_name() : nullptr);
            subtypes.push_back(NestedSubType(cname, k));
        }
        this->depth--;
        return new NestedType(subtypes);
    }
    else if (
        geo_type
        && nopt < (nullable_type + non_nullable_type + array_type + map_type + tuple_type + variant_type + nested_type + geo_type + 1))
    {
        //geo
        const sql_query_grammar::GeoTypes gt = static_cast<sql_query_grammar::GeoTypes>(
            (rg.NextRandomUInt32() % static_cast<uint32_t>(sql_query_grammar::GeoTypes_MAX)) + 1);

        if (tp)
        {
            tp->set_geo(gt);
        }
        return new GeoType(gt);
    }
    else
    {
        assert(0);
    }
    return nullptr;
}

const SQLType * StatementGenerator::RandomNextType(RandomGenerator & rg, const uint32_t allowed_types)
{
    uint32_t col_counter = 0;

    return RandomNextType(rg, allowed_types, col_counter, nullptr);
}

void AppendDecimal(RandomGenerator & rg, std::string & ret, const uint32_t left, const uint32_t right)
{
    ret += rg.NextBool() ? "-" : "";
    if (left > 0)
    {
        std::uniform_int_distribution<uint32_t> next_dist(1, left);
        const uint32_t nlen = next_dist(rg.gen);

        ret += std::max<char>(rg.NextDigit(), '1');
        for (uint32_t j = 1; j < nlen; j++)
        {
            ret += rg.NextDigit();
        }
    }
    else
    {
        ret += "0";
    }
    ret += ".";
    if (right > 0)
    {
        std::uniform_int_distribution<uint32_t> next_dist(1, right);
        const uint32_t nlen = next_dist(rg.gen);

        for (uint32_t j = 0; j < nlen; j++)
        {
            ret += rg.NextDigit();
        }
    }
    else
    {
        ret += "0";
    }
}

static inline void NextFloatingPoint(RandomGenerator & rg, std::string & ret)
{
    const uint32_t next_option = rg.NextLargeNumber();

    if (next_option < 25)
    {
        if (next_option < 17)
        {
            ret += next_option < 9 ? "+" : "-";
        }
        ret += "nan";
    }
    else if (next_option < 49)
    {
        if (next_option < 41)
        {
            ret += next_option < 33 ? "+" : "-";
        }
        ret += "inf";
    }
    else if (next_option < 73)
    {
        if (next_option < 65)
        {
            ret += next_option < 57 ? "+" : "-";
        }
        ret += "0.0";
    }
    else if (next_option < 373)
    {
        ret += std::to_string(rg.NextRandomInt32());
    }
    else if (next_option < 673)
    {
        ret += std::to_string(rg.NextRandomInt64());
    }
    else
    {
        ret += std::to_string(rg.NextRandomDouble());
    }
}

void AppendGeoValue(RandomGenerator & rg, std::string & ret, const sql_query_grammar::GeoTypes & geo_type)
{
    const uint32_t limit = rg.NextLargeNumber() % 10;

    switch (geo_type)
    {
        case sql_query_grammar::GeoTypes::Point:
            ret += "(";
            NextFloatingPoint(rg, ret);
            ret += ",";
            NextFloatingPoint(rg, ret);
            ret += ")";
            break;
        case sql_query_grammar::GeoTypes::Ring:
        case sql_query_grammar::GeoTypes::LineString:
            ret += "[";
            for (uint32_t i = 0; i < limit; i++)
            {
                ret += "(";
                NextFloatingPoint(rg, ret);
                ret += ",";
                NextFloatingPoint(rg, ret);
                ret += ")";
            }
            ret += "]";
            break;
        case sql_query_grammar::GeoTypes::MultiLineString:
        case sql_query_grammar::GeoTypes::Polygon:
            ret += "[";
            for (uint32_t i = 0; i < limit; i++)
            {
                const uint32_t nlines = rg.NextLargeNumber() % 10;

                ret += "[";
                for (uint32_t j = 0; j < nlines; j++)
                {
                    ret += "(";
                    NextFloatingPoint(rg, ret);
                    ret += ",";
                    NextFloatingPoint(rg, ret);
                    ret += ")";
                }
                ret += "]";
            }
            ret += "]";
            break;
        case sql_query_grammar::GeoTypes::MultiPolygon:
            ret += "[";
            for (uint32_t i = 0; i < limit; i++)
            {
                const uint32_t npoligons = rg.NextLargeNumber() % 10;

                ret += "[";
                for (uint32_t j = 0; j < npoligons; j++)
                {
                    const uint32_t nlines = rg.NextLargeNumber() % 10;

                    ret += "[";
                    for (uint32_t k = 0; k < nlines; k++)
                    {
                        ret += "(";
                        NextFloatingPoint(rg, ret);
                        ret += ",";
                        NextFloatingPoint(rg, ret);
                        ret += ")";
                    }
                    ret += "]";
                }
                ret += "]";
            }
            ret += "]";
            break;
    }
}

void StatementGenerator::StrAppendBottomValue(RandomGenerator & rg, std::string & ret, const SQLType * tp)
{
    const IntType * itp;
    const DateType * dtp;
    const DateTimeType * dttp;
    const DecimalType * detp;
    const StringType * stp;
    const EnumType * etp;

    if ((itp = dynamic_cast<const IntType *>(tp)))
    {
        if (itp->is_unsigned)
        {
            switch (itp->size)
            {
                case 8:
                    ret += std::to_string(rg.NextRandomUInt8());
                    break;
                case 16:
                    ret += std::to_string(rg.NextRandomUInt16());
                    break;
                case 32:
                    ret += std::to_string(rg.NextRandomUInt32());
                    break;
                case 64:
                    ret += std::to_string(rg.NextRandomUInt64());
                    break;
                default: {
                    hugeint_t val(rg.NextRandomInt64(), rg.NextRandomUInt64());
                    val.ToString(ret);
                }
            }
        }
        else
        {
            switch (itp->size)
            {
                case 8:
                    ret += std::to_string(rg.NextRandomInt8());
                    break;
                case 16:
                    ret += std::to_string(rg.NextRandomInt16());
                    break;
                case 32:
                    ret += std::to_string(rg.NextRandomInt32());
                    break;
                case 64:
                    ret += std::to_string(rg.NextRandomInt64());
                    break;
                default: {
                    uhugeint_t val(rg.NextRandomUInt64(), rg.NextRandomUInt64());
                    val.ToString(ret);
                }
            }
        }
    }
    else if (dynamic_cast<const FloatType *>(tp))
    {
        NextFloatingPoint(rg, ret);
    }
    else if ((dtp = dynamic_cast<const DateType *>(tp)))
    {
        ret += "'";
        if (dtp->extended)
        {
            rg.NextDate32(ret);
        }
        else
        {
            rg.NextDate(ret);
        }
        ret += "'";
    }
    else if ((dttp = dynamic_cast<const DateTimeType *>(tp)))
    {
        ret += "'";
        if (dttp->extended)
        {
            rg.NextDateTime64(ret);
        }
        else
        {
            rg.NextDateTime(ret);
        }
        ret += "'";
    }
    else if ((detp = dynamic_cast<const DecimalType *>(tp)))
    {
        const uint32_t right = detp->scale.value_or(0), left = detp->precision.value_or(10) - right;

        AppendDecimal(rg, ret, left, right);
    }
    else if ((stp = dynamic_cast<const StringType *>(tp)))
    {
        const uint32_t limit = stp->precision.value_or((rg.NextRandomUInt32() % 10000) + 1);

        rg.NextString(ret, "'", true, limit);
    }
    else if (dynamic_cast<const BoolType *>(tp))
    {
        ret += rg.NextBool() ? "TRUE" : "FALSE";
    }
    else if ((etp = dynamic_cast<const EnumType *>(tp)))
    {
        const EnumValue & nvalue = rg.PickRandomlyFromVector(etp->values);

        ret += nvalue.val;
    }
    else if (dynamic_cast<const UUIDType *>(tp))
    {
        ret += "'";
        rg.NextUUID(ret);
        ret += "'";
    }
    else if (dynamic_cast<const IPv4Type *>(tp))
    {
        ret += "'";
        rg.NextIPv4(ret);
        ret += "'";
    }
    else if (dynamic_cast<const IPv6Type *>(tp))
    {
        ret += "'";
        rg.NextIPv6(ret);
        ret += "'";
    }
    else
    {
        assert(0);
    }
}

void StatementGenerator::StrAppendMap(RandomGenerator & rg, std::string & ret, const MapType * mt)
{
    const uint32_t limit = rg.NextLargeNumber() % 100;

    ret += "map(";
    for (uint32_t i = 0; i < limit; i++)
    {
        if (i != 0)
        {
            ret += ", ";
        }
        StrAppendAnyValueInternal(rg, ret, mt->key);
        ret += ",";
        StrAppendAnyValueInternal(rg, ret, mt->value);
    }
    ret += ")";
}

void StatementGenerator::StrAppendArray(RandomGenerator & rg, std::string & ret, const ArrayType * at)
{
    const uint32_t limit = rg.NextLargeNumber() % 100;

    ret += "[";
    for (uint32_t i = 0; i < limit; i++)
    {
        if (i != 0)
        {
            ret += ", ";
        }
        StrAppendAnyValueInternal(rg, ret, at->subtype);
    }
    ret += "]";
}

void StatementGenerator::StrAppendTuple(RandomGenerator & rg, std::string & ret, const TupleType * at)
{
    ret += "(";
    for (uint32_t i = 0; i < at->subtypes.size(); i++)
    {
        if (i != 0)
        {
            ret += ", ";
        }
        StrAppendAnyValueInternal(rg, ret, at->subtypes[i].subtype);
    }
    ret += ")";
}

void StatementGenerator::StrAppendVariant(RandomGenerator & rg, std::string & ret, const VariantType * vtp)
{
    if (vtp->subtypes.empty())
    {
        ret += "NULL";
    }
    else
    {
        StrAppendAnyValueInternal(rg, ret, rg.PickRandomlyFromVector(vtp->subtypes));
    }
}

void StrBuildJSONArray(RandomGenerator & rg, const int jdepth, const int jwidth, std::string & ret)
{
    std::uniform_int_distribution<int> jopt(1, 3);
    int nelems = 0, next_width = 0;

    if (jwidth)
    {
        std::uniform_int_distribution<int> alen(0, jwidth);
        nelems = alen(rg.gen);
    }
    ret += "[";
    next_width = nelems;
    for (int j = 0; j < nelems; j++)
    {
        if (j != 0)
        {
            ret += ",";
        }
        if (jdepth)
        {
            const int noption = jopt(rg.gen);

            switch (noption)
            {
                case 1: //object
                    StrBuildJSON(rg, jdepth - 1, next_width, ret);
                    break;
                case 2: //array
                    StrBuildJSONArray(rg, jdepth - 1, next_width, ret);
                    break;
                case 3: //others
                    StrBuildJSONElement(rg, ret);
                    break;
                default:
                    assert(0);
            }
        }
        else
        {
            StrBuildJSONElement(rg, ret);
        }
        next_width--;
    }
    ret += "]";
}

void StrBuildJSONElement(RandomGenerator & rg, std::string & ret)
{
    std::uniform_int_distribution<int> opts(1, 16);
    const int noption = opts(rg.gen);

    switch (noption)
    {
        case 1:
            ret += "false";
            break;
        case 2:
            ret += "true";
            break;
        case 3:
            ret += "null";
            break;
        case 4: //large number
            ret += std::to_string(rg.NextRandomInt64());
            break;
        case 5: //large unsigned number
            ret += std::to_string(rg.NextRandomUInt64());
            break;
        case 6:
        case 7: { //small number
            std::uniform_int_distribution<int> numbers(-1000, 1000);
            ret += std::to_string(numbers(rg.gen));
        }
        break;
        case 8: //date
            ret += '"';
            if (noption < 251)
            {
                rg.NextDate(ret);
            }
            else if (noption < 301)
            {
                rg.NextDate32(ret);
            }
            else if (noption < 351)
            {
                rg.NextDateTime(ret);
            }
            else
            {
                rg.NextDateTime64(ret);
            }
            ret += '"';
            break;
        case 9: { //decimal
            std::uniform_int_distribution<uint32_t> next_dist(0, 30);
            const uint32_t left = next_dist(rg.gen), right = next_dist(rg.gen);

            AppendDecimal(rg, ret, left, right);
        }
        break;
        case 10:
        case 11:
        case 12: //string
            rg.NextString(ret, "\"", false, (rg.NextRandomUInt32() % 10000) + 1);
            break;
        case 13: //uuid
            ret += '"';
            rg.NextUUID(ret);
            ret += '"';
            break;
        case 14: //ipv4
            ret += '"';
            rg.NextIPv4(ret);
            ret += '"';
            break;
        case 15: //ipv6
            ret += '"';
            rg.NextIPv6(ret);
            ret += '"';
            break;
        case 16: //double
            ret += std::to_string(rg.NextRandomDouble());
            break;
        case 17: { //geo
            const sql_query_grammar::GeoTypes gt = static_cast<sql_query_grammar::GeoTypes>(
                (rg.NextRandomUInt32() % static_cast<uint32_t>(sql_query_grammar::GeoTypes_MAX)) + 1);

            ret += "'";
            AppendGeoValue(rg, ret, gt);
            ret += "'";
        }
        break;
        default:
            assert(0);
    }
}

void StrBuildJSON(RandomGenerator & rg, const int jdepth, const int jwidth, std::string & ret)
{
    ret += "{";
    if (jdepth && jwidth && rg.NextSmallNumber() < 9)
    {
        std::uniform_int_distribution<int> childd(1, jwidth);
        const int nchildren = childd(rg.gen);

        for (int i = 0; i < nchildren; i++)
        {
            std::uniform_int_distribution<int> jopt(1, 3);
            const int noption = jopt(rg.gen);

            if (i != 0)
            {
                ret += ",";
            }
            ret += "\"";
            rg.NextJsonCol(ret);
            ret += "\":";
            switch (noption)
            {
                case 1: //object
                    StrBuildJSON(rg, jdepth - 1, jwidth, ret);
                    break;
                case 2: //array
                    StrBuildJSONArray(rg, jdepth - 1, jwidth, ret);
                    break;
                case 3: //others
                    StrBuildJSONElement(rg, ret);
                    break;
                default:
                    assert(0);
            }
        }
    }
    ret += "}";
}

void StatementGenerator::StrAppendAnyValueInternal(RandomGenerator & rg, std::string & ret, const SQLType * tp)
{
    const MapType * mt;
    const Nullable * nl;
    const ArrayType * at;
    const TupleType * ttp;
    const VariantType * vtp;
    const LowCardinality * lc;
    const GeoType * gtp;
    const uint32_t ndefault = rg.NextMediumNumber();

    if (ndefault < 5)
    {
        ret += "NULL";
    }
    else if (ndefault == 5)
    {
        ret += "DEFAULT";
    }
    else if (
        dynamic_cast<const IntType *>(tp) || dynamic_cast<const FloatType *>(tp) || dynamic_cast<const DateType *>(tp)
        || dynamic_cast<const DateTimeType *>(tp) || dynamic_cast<const DecimalType *>(tp) || dynamic_cast<const StringType *>(tp)
        || dynamic_cast<const BoolType *>(tp) || dynamic_cast<const EnumType *>(tp) || dynamic_cast<const UUIDType *>(tp)
        || dynamic_cast<const IPv4Type *>(tp) || dynamic_cast<const IPv6Type *>(tp))
    {
        StrAppendBottomValue(rg, ret, tp);
    }
    else if ((lc = dynamic_cast<const LowCardinality *>(tp)))
    {
        StrAppendAnyValueInternal(rg, ret, lc->subtype);
    }
    else if ((nl = dynamic_cast<const Nullable *>(tp)))
    {
        StrAppendAnyValueInternal(rg, ret, nl->subtype);
    }
    else if (dynamic_cast<const JSONType *>(tp))
    {
        std::uniform_int_distribution<int> dopt(1, this->fc.max_depth), wopt(1, this->fc.max_width);

        ret += "'";
        StrBuildJSON(rg, dopt(rg.gen), wopt(rg.gen), ret);
        ret += "'";
    }
    else if (dynamic_cast<const DynamicType *>(tp))
    {
        uint32_t col_counter = 0;
        const SQLType * next = RandomNextType(rg, allow_nullable | allow_json, col_counter, nullptr);

        StrAppendAnyValueInternal(rg, ret, next);
        delete next;
    }
    else if ((gtp = dynamic_cast<const GeoType *>(tp)))
    {
        AppendGeoValue(rg, ret, gtp->geo_type);
    }
    else if (this->depth == this->fc.max_depth)
    {
        ret += "1";
    }
    else if ((mt = dynamic_cast<const MapType *>(tp)))
    {
        this->depth++;
        StrAppendMap(rg, ret, mt);
        this->depth--;
    }
    else if ((at = dynamic_cast<const ArrayType *>(tp)))
    {
        this->depth++;
        StrAppendArray(rg, ret, at);
        this->depth--;
    }
    else if ((ttp = dynamic_cast<const TupleType *>(tp)))
    {
        this->depth++;
        StrAppendTuple(rg, ret, ttp);
        this->depth--;
    }
    else if ((vtp = dynamic_cast<const VariantType *>(tp)))
    {
        this->depth++;
        StrAppendVariant(rg, ret, vtp);
        this->depth--;
    }
    else
    {
        //no nested types here
        assert(0);
    }
}

void StatementGenerator::StrAppendAnyValue(RandomGenerator & rg, std::string & ret, const SQLType * tp)
{
    StrAppendAnyValueInternal(rg, ret, tp);
    if (rg.NextSmallNumber() < 7)
    {
        ret += "::";
        tp->TypeName(ret, false);
    }
}

}
